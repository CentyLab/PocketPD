/**
 * @file profile_picker_stage.h
 * @brief Profile-selection stage for NormalStage entry.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include "menu_stage.h"
#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class ProfilePickerStage : public App::Stage, public App::UseLog<ProfilePickerStage> {
    private:
        enum class PowerSourceType : uint8_t { NON_PD, PD };

        using Display = tempo::Display;

        Display& m_display;
        PdSinkController& m_pd_sink;

        int m_cursor = 0;
        int m_pending_cursor = 0;
        tempo::TimeoutTimer m_passthrough_timeout;

        void commit(Conductor& conductor) {
            if (m_pd_sink.pdo_count() <= 0) {
                return; // empty-PDO fallback: long-press is a no-op
            }
            m_cursor = m_pending_cursor;
            conductor.reset_root<NormalStage>(static_cast<int8_t>(m_cursor));
        }

    public:
        static constexpr const char* LOG_TAG = "ProfilePicker";

        ProfilePickerStage(tempo::Display& display, PdSinkController& pd_sink)
            : m_display(display), m_pd_sink(pd_sink) {}

        const char* name() const override {
            return "PROFILE_PICKER";
        }

        int cursor_index() const {
            return m_cursor;
        }

        PowerSourceType power_source_type() const {
            return (m_pd_sink.pdo_count() > 0) ? PowerSourceType::PD : PowerSourceType::NON_PD;
        }

        void on_enter(Conductor&, uint32_t now_ms) override {
            m_pending_cursor = m_cursor;
            render_pdo_list();

            if (power_source_type() == PowerSourceType::NON_PD) {
                m_passthrough_timeout.set(now_ms, PICKER_PASSTHROUGH_AUTO_MS);
            } else {
                m_passthrough_timeout.disarm();
            }
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            if (m_passthrough_timeout.reached(now_ms)) {
                conductor.reset_root<NormalStage>(static_cast<int8_t>(-1));
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            // Passthrough-only screen: any user input drops to NormalStage in passthrough.
            if (power_source_type() == PowerSourceType::NON_PD) {
                auto pass_handler = tempo::overloaded{
                    [&](const ButtonEvent&) { conductor.reset_root<NormalStage>(-1); },
                    [&](const EncoderEvent& evt) { conductor.reset_root<NormalStage>(-1); },
                    [](const auto&) {},
                };
                std::visit(pass_handler, event);
                return;
            }

            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    const int count = m_pd_sink.pdo_count();
                    int next = std::clamp(m_pending_cursor + evt.delta, 0, count - 1);
                    if (next == m_pending_cursor) {
                        return;
                    }

                    m_pending_cursor = next;
                    render_pdo_list();
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.pop();
                        return;
                    }

                    if (evt.id == ButtonId::ENCODER && evt.gesture == Gesture::LONG) {
                        commit(conductor);
                    }
                },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }

        void render_pdo_list() {
            m_display.clear();

            if (power_source_type() == PowerSourceType::NON_PD) {
                const char* line1 = "Non-PD Source";
                const char* line2 = "Passthrough Mode";
                const auto x1 = static_cast<uint8_t>((128 - m_display.text_width(line1)) / 2);
                const auto x2 = static_cast<uint8_t>((128 - m_display.text_width(line2)) / 2);
                m_display.draw_text(x1, 28, line1);
                m_display.draw_text(x2, 44, line2);
                m_display.flush();
                return;
            }

            std::array<char, 32> buffer{};

            const int count = m_pd_sink.pdo_count();
            for (int row_index = 0; row_index < count; row_index++) {
                const auto y = static_cast<uint8_t>(9 * (row_index + 1));

                if (row_index == m_pending_cursor) {
                    m_display.draw_text(0, y, ">");
                }

                std::array<char, 12> vbuf{};
                if (m_pd_sink.is_index_fixed(row_index)) {
                    const int v = m_pd_sink.pdo_max_voltage_mv(row_index) / 1000;
                    const int a = m_pd_sink.pdo_max_current_ma(row_index) / 1000;

                    std::snprintf(vbuf.data(), vbuf.size(), "%7d.0V", v);
                    std::snprintf(
                        buffer.data(), buffer.size(), "PDO %-12s%dA", vbuf.data(), a
                    );

                    m_display.draw_text(10, y, buffer.data());
                } else if (m_pd_sink.is_index_pps(row_index)) {
                    const int min_mv = m_pd_sink.pdo_min_voltage_mv(row_index);
                    const int max_mv = m_pd_sink.pdo_max_voltage_mv(row_index);

                    const int min_v = min_mv / 1000;
                    const int min_dv = (min_mv % 1000) / 100;

                    const int max_v = max_mv / 1000;
                    const int max_dv = (max_mv % 1000) / 100;

                    const int a = m_pd_sink.pdo_max_current_ma(row_index) / 1000;

                    std::snprintf(
                        vbuf.data(), vbuf.size(), "%2d.%d-%2d.%dV",
                        min_v, min_dv, max_v, max_dv
                    );
                    std::snprintf(
                        buffer.data(), buffer.size(), "PPS %-12s%dA", vbuf.data(), a
                    );

                    m_display.draw_text(10, y, buffer.data());
                }
            }

            m_display.flush();
        }
    };

} // namespace pocketpd
