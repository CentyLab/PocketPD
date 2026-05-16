/**
 * @file profile_picker_stage.h
 * @brief Profile-selection stage. Renders the source PDO list with an encoder-driven cursor.
 * The user commits a profile by long-pressing the encoder, or exits without changing by long-
 * pressing L. There is no auto-transition to Normal — the user must choose.
 */
#pragma once

#include <tempo/bus/visit.h>
#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    void render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink, int cursor = -1);

    class ProfilePickerStage : public App::Stage, public App::UseLog<ProfilePickerStage> {
    private:
        using Display = tempo::Display;

        Display& m_display;
        PdSinkController& m_pd_sink;

        int m_cursor = 0;
        int m_pending_cursor = 0;

        void commit(Conductor& conductor) {
            if (m_pd_sink.pdo_count() <= 0) {
                return; // empty-PDO fallback: long-press is a no-op
            }
            m_cursor = m_pending_cursor;
            conductor.request<NormalStage>(static_cast<int8_t>(m_cursor));
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

        void on_enter(Conductor&) override {
            m_pending_cursor = m_cursor;
            render_pdo_list(m_display, m_pd_sink, m_pending_cursor);
        }

        void on_tick(Conductor&, uint32_t) override {}

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    const int count = m_pd_sink.pdo_count();
                    if (count <= 0 || evt.delta == 0) {
                        return;
                    }

                    int next = std::clamp(m_pending_cursor + evt.delta, 0, count - 1);
                    if (next == m_pending_cursor) {
                        return;
                    }

                    m_pending_cursor = next;
                    render_pdo_list(m_display, m_pd_sink, m_pending_cursor);
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.request<NormalStage>();
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
    };

    inline void
    render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink, int cursor) {
        display.clear();

        const int count = pd_sink.pdo_count();
        if (count == 0) {
            const char* msg = "[No Profile Detected]";
            const auto w = display.text_width(msg);
            const auto x = static_cast<uint8_t>((128 - w) / 2);
            display.draw_text(x, 34, msg);
            display.flush();
            return;
        }

        std::array<char, 32> buffer{};

        for (int row_index = 0; row_index < count; row_index++) {
            const auto y = static_cast<uint8_t>(9 * (row_index + 1));

            if (row_index == cursor) {
                display.draw_text(0, y, ">");
            }

            if (pd_sink.is_index_fixed(row_index)) {
                const int v = pd_sink.pdo_max_voltage_mv(row_index) / 1000;
                const int a = pd_sink.pdo_max_current_ma(row_index) / 1000;

                const char* fmt = "PDO: %dV @ %dA";
                std::snprintf(buffer.data(), buffer.size(), fmt, v, a);

                display.draw_text(5, y, buffer.data());
            } else if (pd_sink.is_index_pps(row_index)) {
                const int min_mv = pd_sink.pdo_min_voltage_mv(row_index);
                const int max_mv = pd_sink.pdo_max_voltage_mv(row_index);

                const int min_v = min_mv / 1000;
                const int min_dv = (min_mv % 1000) / 100;

                const int max_v = max_mv / 1000;
                const int max_dv = (max_mv % 1000) / 100;

                const int a = pd_sink.pdo_max_current_ma(row_index) / 1000;

                const char* fmt = "PPS: %d.%dV~%d.%dV @ %dA";
                std::snprintf(buffer.data(), buffer.size(), fmt, min_v, min_dv, max_v, max_dv, a);

                display.draw_text(5, y, buffer.data());
            }
        }

        display.flush();
    }

} // namespace pocketpd
