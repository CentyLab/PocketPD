/**
 * @file energy_task.h
 * @brief Calculate running total of energy outputs and publish them across the application
 *
 */
#pragma once

#include <cstdint>
#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/output_gate.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class EnergyTask : public App::StageScopedTask, public App::UsePublisher<EnergyTask> {
    private:
        OutputGate& m_output_gate;

        double m_accumulated_wh = 0.0;
        double m_accumulated_ah = 0.0;
        uint32_t m_accumulated_ms = 0;

        uint32_t m_last_update_ms = 0;
        bool m_session_active = false;

    public:
        static constexpr uint32_t PERIOD_MS = 100;

        EnergyTask(OutputGate& output_gate)
            : App::StageScopedTask(
                  PERIOD_MS, App::StageMask::of<NormalStage, EnergyStage, ProfilePickerStage>()),
              m_output_gate(output_gate) {}

        const char* name() const override {
            return "EnergyTask";
        }

        double accumulated_wh() const {
            return m_accumulated_wh;
        }

        double accumulated_ah() const {
            return m_accumulated_ah;
        }

        uint32_t total_seconds() const {
            return m_accumulated_ms / 1000;
        }

        void on_event(const Event& event, uint32_t) override {
            if (const auto sensor = std::get_if<SensorEvent>(&event)) {
                integrate(sensor->snapshot);
            }
        }

        void on_tick(uint32_t) override {
            const EnergyEvent event = {
                m_accumulated_wh,
                m_accumulated_ah,
                total_seconds(),
            };

            publish(event);
        }

    private:
        void integrate(const SensorSnapshot& s) {
            if (!m_output_gate.is_enabled()) {
                m_session_active = false;
                return;
            }

            // Output just turned on so skipping integration (no prior delta) as we only want to
            // integrate over a continuous range
            if (!m_session_active) {
                m_session_active = true;
                m_last_update_ms = s.timestamp_ms;
                return;
            }

            const uint32_t delta_ms = s.timestamp_ms - m_last_update_ms;
            m_last_update_ms = s.timestamp_ms;

            // Stall guard: if the sensor stopped firing for >= 1 s, the load may have changed
            // during the gap — discard rather than attribute energy to a possibly stale V/I
            // reading.
            if (delta_ms >= 1000) {
                return;
            }

            m_accumulated_ms += delta_ms;
            const double voltage_v = s.vbus_mv / 1000.0;
            const double current_a = s.current_ma / 1000.0;
            const double delta_h = delta_ms / 3600000.0;
            m_accumulated_wh += voltage_v * current_a * delta_h;
            m_accumulated_ah += current_a * delta_h;
        }
    };

} // namespace pocketpd
