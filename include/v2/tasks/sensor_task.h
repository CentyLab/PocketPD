/**
 * @file sensor_task.h
 * @brief Reads PowerMonitor at 33 ms while NormalStage is active and copies
 * the result into NormalStage's SensorSnapshot.
 */
#pragma once

#include <cstdint>

#include "v2/app.h"
#include "v2/hal/power_monitor.h"
#include "v2/stages/normal_stage.h"

namespace pocketpd {

    class SensorTask : public App::StageScopedTask {
    private:
        PowerMonitor& m_monitor;
        NormalStage& m_normal;

    public:
        static constexpr uint32_t PERIOD_MS = 33;

        SensorTask(PowerMonitor& monitor, NormalStage& normal)
            : App::StageScopedTask(PERIOD_MS, App::StageMask::of<NormalStage>()),
              m_monitor(monitor),
              m_normal(normal) {}

        /// Test-driven entry point — bypasses the periodic gate.
        void poll(uint32_t now_ms) {
            const auto reading = m_monitor.read();
            m_normal.sensor_snapshot() = SensorSnapshot{
                now_ms,
                reading.mv,
                reading.ma,
                reading.valid,
            };
        }

    protected:
        void on_tick(uint32_t now_ms) override {
            poll(now_ms);
        }
    };

} // namespace pocketpd
