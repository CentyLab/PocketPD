/**
 * @file sensor_task.h
 * @brief Reads PowerMonitor at 33 ms while NormalStage is active and publishes
 * a `SensorEvent` carrying the snapshot. NormalStage consumes the event.
 */
#pragma once

#include <cstdint>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/power_monitor.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class SensorTask : public App::StageScopedTask, public App::UsePublisher<SensorTask> {
    private:
        PowerMonitor& m_monitor;

    public:
        static constexpr uint32_t PERIOD_MS = 33;

        explicit SensorTask(PowerMonitor& monitor)
            : App::StageScopedTask(
                  PERIOD_MS,
                  App::StageMask::of<NormalStage, EnergyStage, ProfilePickerStage>()
              ),
              m_monitor(monitor) {}

        void on_tick(uint32_t now_ms) override {
            const auto reading = m_monitor.read();
            if (!reading.valid) {
                return;
            }
            publish(SensorEvent{SensorSnapshot{now_ms, reading.mv, reading.ma}});
        }
    };

} // namespace pocketpd
