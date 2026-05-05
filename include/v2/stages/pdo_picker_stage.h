/**
 * @file pdo_picker_stage.h
 * @brief PDO picker / capability list stage. Two modes via payload-passing transition:
 * - REVIEW renders the source PDO list and waits for input;
 * - SELECT renders the same list with an encoder-driven cursor and lets the user commit a profile
 * via long-press of select-VI.
 *
 * Issue #32: SELECT entry disables the load switch so the user cannot change profile
 * mid-delivery.
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
#include "v2/hal/output_gate.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/stages/normal_stage.h"

namespace pocketpd {

    void render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink, int cursor = -1);

    class PdoPickerStage : public App::Stage, public App::UseLog<PdoPickerStage> {
    public:
        enum class Mode : uint8_t { REVIEW, SELECT };

    private:
        using Display = tempo::Display;

        Display& m_display;
        PdSinkController& m_pd_sink;
        OutputGate& m_output_gate;

        Mode m_mode = Mode::REVIEW;
        int m_cursor_index = 0;

        tempo::TimeoutTimer m_review_timeout;

        Profile profile_for_charger() const {
            return m_pd_sink.pps_count() > 0 ? Profile::PPS : Profile::PDO;
        }

        Profile profile_at_cursor() const {
            return m_pd_sink.is_index_pps(m_cursor_index) ? Profile::PPS : Profile::PDO;
        }

    public:
        static constexpr const char* LOG_TAG = "PdoPick";

        PdoPickerStage(tempo::Display& display, PdSinkController& pd_sink, OutputGate& output_gate)
            : m_display(display), m_pd_sink(pd_sink), m_output_gate(output_gate) {}

        const char* name() const override {
            return "PDO_PICKER";
        }

        Mode mode() const {
            return m_mode;
        }

        void prepare(Mode mode) {
            m_mode = mode;
        }

        void on_enter(Conductor&) override {
            switch (m_mode) {
            case Mode::REVIEW:
                m_review_timeout.disarm();
                render_pdo_list(m_display, m_pd_sink);
                break;
            case Mode::SELECT:
                m_cursor_index = 0;
                m_output_gate.disable();
                render_pdo_list(m_display, m_pd_sink, m_cursor_index);
                break;
            }
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            if (m_mode != Mode::REVIEW) {
                return;
            }

            if (!m_review_timeout.armed()) {
                m_review_timeout.set(now_ms, PDOPICKER_REVIEW_TO_NORMAL_MS);
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
                [&](const EncoderEvent& evt) {
                    if (evt.delta != 0) {
                        conductor.request<PdoPickerStage>(Mode::SELECT);
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

                    int next = std::clamp(m_cursor_index + evt.delta, 0, count - 1);
                    if (next == m_cursor_index) {
                        return;
                    }

                    m_cursor_index = next;
                    render_pdo_list(m_display, m_pd_sink, m_cursor_index);
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id != ButtonId::SELECT_VI || evt.gesture != Gesture::LONG) {
                        return;
                    }
                    if (m_pd_sink.pdo_count() <= 0) {
                        return; // empty-PDO fallback: long-press is a no-op
                    }

                    conductor.request<NormalStage>(profile_at_cursor());
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
