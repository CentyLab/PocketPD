/**
 * @file profile_picker_stage.h
 * @brief Profile-selection / capability-list stage. Two modes via payload-passing transition:
 * - REVIEW renders the source PDO list and waits for input;
 * - SELECT renders the same list with an encoder-driven cursor and lets the user commit a profile
 * via long-press of the encoder button.
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
#include "v2/state.h"

namespace pocketpd {

    void render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink, int cursor = -1);

    class ProfilePickerStage : public App::Stage, public App::UseLog<ProfilePickerStage> {
    public:
        using Mode = ProfilePickerMode;

    private:
        using Display = tempo::Display;

        Display& m_display;
        PdSinkController& m_pd_sink;

        Mode m_mode = Mode::REVIEW;
        int m_cursor = 0;
        int m_pending_cursor = 0;

        tempo::TimeoutTimer m_review_timeout;

        Profile profile_for_charger() const {
            return m_pd_sink.pps_count() > 0 ? Profile::PPS : Profile::PDO;
        }

        Profile profile_at(int cursor) const {
            return m_pd_sink.is_index_pps(cursor) ? Profile::PPS : Profile::PDO;
        }

        void commit(Conductor& conductor) {
            if (m_pd_sink.pdo_count() <= 0) {
                return; // empty-PDO fallback: long-press is a no-op
            }
            m_cursor = m_pending_cursor;
            conductor.request<NormalStage>(profile_at(m_cursor));
        }

    public:
        static constexpr const char* LOG_TAG = "ProfilePicker";

        ProfilePickerStage(tempo::Display& display, PdSinkController& pd_sink)
            : m_display(display), m_pd_sink(pd_sink) {}

        const char* name() const override {
            return "PROFILE_PICKER";
        }

        Mode mode() const {
            return m_mode;
        }

        int cursor_index() const {
            return m_cursor;
        }

        void prepare(Mode mode) {
            m_mode = mode;
        }

        void on_enter(Conductor&) override {
            m_review_timeout.disarm();

            switch (m_mode) {
            case Mode::REVIEW:
                render_pdo_list(m_display, m_pd_sink);
                break;
            case Mode::SELECT:
                m_pending_cursor = m_cursor;
                render_pdo_list(m_display, m_pd_sink, m_pending_cursor);
                break;
            }
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            if (m_mode != Mode::REVIEW) {
                return;
            }

            if (!m_review_timeout.armed()) {
                m_review_timeout.set(now_ms, PROFILE_PICKER_REVIEW_TO_NORMAL_MS);
                return;
            }

            if (m_review_timeout.reached(now_ms)) {
                conductor.request<NormalStage>(profile_for_charger());
                return;
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            switch (m_mode) {
            case Mode::REVIEW:
                handle_review_event(conductor, event);
                break;
            case Mode::SELECT:
                handle_select_event(conductor, event);
                break;
            }
        }

    private:
        void handle_review_event(Conductor& conductor, const Event& event) {
            auto handler = tempo::overloaded{
                [&](const ButtonEvent& evt) {
                    if (evt.gesture == Gesture::SHORT) {
                        conductor.request<NormalStage>(profile_for_charger());
                    }
                },
                /**
                 * @brief In review mode, turning the encoder auto-enters the SELECT mode
                 *
                 * @param evt incoming EncoderEvent
                 */
                [&](const EncoderEvent& evt) {
                    if (evt.delta != 0) {
                        conductor.request<ProfilePickerStage>(ProfilePickerMode::SELECT);
                    }
                },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }

        void handle_select_event(Conductor& conductor, const Event& event) {
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
                    // Exit — do nothing
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
            display.draw_text(8, 34, "No Profile Detected");
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
