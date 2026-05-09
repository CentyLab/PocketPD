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
#include "v2/state.h"

namespace pocketpd {

    class EnergyStage : public App::Stage, public App::UseLog<EnergyStage> {
    private:
        using Display = tempo::Display;

        Display& m_display;
        OutputGate& m_output_gate;
        SensorSnapshot m_snapshot{};
        bool m_snapshot_seeded = false;
        EnergyEvent m_energy{};
        int8_t m_active_pdo_index = -1;
        tempo::IntervalTimer m_render_interval{40};
        uint8_t m_arrow_frame = 0;

        static constexpr uint32_t SENSOR_EMA_DEN = 4;

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

        void prepare(int8_t pdo_index = -1) {
            m_active_pdo_index = pdo_index;
        }

        void on_enter(Conductor&) override {
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
                [&](const ButtonEvent& evt) {
                    if (evt.id != ButtonId::R) {
                        return;
                    }
                    if (evt.gesture == Gesture::SHORT) {
                        m_output_gate.toggle();
                        return;
                    }
                    if (evt.gesture == Gesture::LONG) {
                        conductor.request<NormalStage>(m_active_pdo_index);
                    }
                },
                [&](const SensorEvent& evt) { ema_filter(evt.snapshot); },
                [&](const EnergyEvent& evt) { m_energy = evt; },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }

    private:
        /**
         * @brief Apply EMA smoothing to displayed mV / mA. Same shape as
         * NormalStage so the readout is consistent across screens.
         */
        void ema_filter(const SensorSnapshot& s) {
            if (!m_snapshot_seeded) {
                m_snapshot = s;
                m_snapshot_seeded = true;
                return;
            }
            const uint32_t a = SENSOR_EMA_DEN - 1;
            m_snapshot.vbus_mv = (m_snapshot.vbus_mv * a + s.vbus_mv) / SENSOR_EMA_DEN;
            m_snapshot.current_ma = (m_snapshot.current_ma * a + s.current_ma) / SENSOR_EMA_DEN;
            m_snapshot.timestamp_ms = s.timestamp_ms;
        }

        EnergyViewModel build_view_model() const {
            return EnergyViewModel{
                .snapshot = m_snapshot,
                .accumulated_wh = m_energy.accumulated_wh,
                .accumulated_ah = m_energy.accumulated_ah,
                .total_seconds = m_energy.total_seconds,
                .arrow_frame = m_arrow_frame,
                .output_enabled = m_output_gate.is_enabled(),
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
