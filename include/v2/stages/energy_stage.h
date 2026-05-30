/**
 * @file energy_stage.h
 * @brief Energy display stage.
 *
 * Entered via R LONG from `NormalStage`. Renders Wh / Ah / on-time alongside live V / A. The active
 * PDO index is passed via `prepare(int8_t)` so R LONG returns to `NormalStage` with the same
 * profile.
 *
 * Inputs handled:
 *   - R SHORT: toggle `OutputGate` (output stays editable while viewing energy).
 *   - R LONG:  return to `NormalStage(active_pdo_index)`.
 *   - All other gestures + EncoderEvents are ignored.
 */
#pragma once

#include <tempo/bus/visit.h>
#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/output_gate.h"
#include "v2/images.h"
#include "v2/stages/energy/energy_view.h"

namespace pocketpd {

    class EnergyStage : public App::Stage, public App::UseLog<EnergyStage> {
    private:
        using Display = tempo::Display;

        Display& m_display;
        OutputGate& m_output_gate;
        LoadReading m_load_reading{};
        bool m_load_init = false;
        EnergyEvent m_energy{};
        int8_t m_active_pdo_index = -1;
        tempo::IntervalTimer m_render_interval{40};
        uint8_t m_arrow_frame = 0;
        bool m_locked = false;

    public:
        static constexpr const char* LOG_TAG = "EnergyStage";

        EnergyStage(Display& display, OutputGate& output_gate)
            : m_display(display), m_output_gate(output_gate) {}

        const char* name() const override {
            return LOG_TAG;
        }

        int8_t active_pdo_index() const {
            return m_active_pdo_index;
        }

        bool locked() const {
            return m_locked;
        }

        void prepare(int8_t pdo_index = -1) {
            m_active_pdo_index = pdo_index;
        }

        void on_enter(Conductor&, uint32_t) override {
            m_locked = false;
            log.info("Entered Energy screen pdo_index={}", m_active_pdo_index);
            draw();
        }

        void on_tick(Conductor&, uint32_t now_ms) override {
            if (m_render_interval.tick(now_ms)) {
                draw();
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const ButtonEvent& event) {
                    // L+R combo always reachable; must precede lock guard so a locked screen can
                    // unlock.
                    if (event.lr_long()) {
                        m_locked = !m_locked;
                        return;
                    }
                    if (m_locked) {
                        return;
                    }

                    if (event.r_short()) {
                        m_output_gate.toggle();
                        return;
                    }
                    if (event.r_long()) {
                        conductor.pop();
                    }
                },
                [&](const SensorEvent& event) {
                    if (m_load_init) {
                        m_load_reading = m_load_reading.ema(event.load);
                    } else {
                        m_load_reading = event.load;
                        m_load_init = true;
                    }
                },
                [&](const EnergyEvent& evt) { m_energy = evt; },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }

    private:
        EnergyViewModel build_view_model() const {
            return EnergyViewModel{
                .load_reading = m_load_reading,
                .accumulated_wh = m_energy.accumulated_wh,
                .accumulated_ah = m_energy.accumulated_ah,
                .total_seconds = m_energy.total_seconds,
                .arrow_frame = m_arrow_frame,
                .output_enabled = m_output_gate.is_enabled(),
                .locked = m_locked,
            };
        }

        void draw() {
            EnergyView::render(m_display, build_view_model());
            if (m_output_gate.is_enabled()) {
                m_arrow_frame = (m_arrow_frame + 1) % pocketpd::bitmap::ARROW_FRAMES.size();
            }
        }
    };

} // namespace pocketpd
