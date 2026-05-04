/**
 * @file pdo_picker_stage.h
 * @brief PDO picker / capability list stage. Two modes via tempo payload-passing transition:
 * REVIEW renders the source PDO list and waits for input; SELECT renders the same list with an
 * encoder-driven cursor.
 *
 * REVIEW renders once on entry — the PDO list is fixed for the lifetime of the negotiation, so
 * no on_tick redraw is needed. SELECT re-renders on each cursor move. Output gating on SELECT
 * entry, REVIEW exit transitions, and long-press commit are not yet wired.
 */
#pragma once

#include <tempo/bus/publisher.h>
#include <tempo/bus/visit.h>
#include <tempo/hardware/display.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <variant>
#include <algorithm>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"

namespace pocketpd {

    void render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink, int cursor = -1);

    class PdoPickerStage : public App::Stage, public App::UseLog<PdoPickerStage> {
    public:
        enum class Mode : uint8_t { REVIEW, SELECT };

    private:
        tempo::Display& m_display;
        PdSinkController& m_pd_sink;
        Mode m_mode = Mode::REVIEW;
        int m_cursor = 0;

    public:
        static constexpr const char* LOG_TAG = "PdoPick";

        PdoPickerStage(tempo::Display& display, PdSinkController& pd_sink)
            : m_display(display), m_pd_sink(pd_sink) {}

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
            if (m_mode == Mode::REVIEW) {
                render_pdo_list(m_display, m_pd_sink);
            } else {
                m_cursor = 0;
                render_pdo_list(m_display, m_pd_sink, m_cursor);
            }
        }

        void on_event(Conductor&, const Event& event, uint32_t) override {
            if (m_mode != Mode::SELECT) {
                return;
            }

            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    const int count = m_pd_sink.pdo_count();
                    if (count <= 0 || evt.delta == 0) {
                        return;
                    }

                    int next = std::clamp(m_cursor + evt.delta, 0, count - 1);
                    if (next == m_cursor) {
                        return;
                    }

                    m_cursor = next;
                    render_pdo_list(m_display, m_pd_sink, m_cursor);
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
                std::snprintf(buffer.data(), buffer.size(), "PDO: %dV @ %dA", v, a);
                display.draw_text(5, y, buffer.data());
            } else if (pd_sink.is_index_pps(row_index)) {
                const int min_mv = pd_sink.pdo_min_voltage_mv(row_index);
                const int max_mv = pd_sink.pdo_max_voltage_mv(row_index);

                const int min_v = min_mv / 1000;
                const int min_dv = (min_mv % 1000) / 100;

                const int max_v = max_mv / 1000;
                const int max_dv = (max_mv % 1000) / 100;

                const int a = pd_sink.pdo_max_current_ma(row_index) / 1000;
                std::snprintf(
                    buffer.data(),
                    buffer.size(),
                    "PPS: %d.%dV~%d.%dV @ %dA",
                    min_v,
                    min_dv,
                    max_v,
                    max_dv,
                    a
                );

                display.draw_text(5, y, buffer.data());
            }
        }

        display.flush();
    }

} // namespace pocketpd
