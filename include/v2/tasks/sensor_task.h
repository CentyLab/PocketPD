/**
 * @file sensor_task.h
 * @brief Polls PowerMonitor and SupplyVoltageSource at 33 ms while NormalStage,
 * EnergyStage, or ProfilePickerStage is active. Publishes a fused `SensorEvent`.
 */
#pragma once

#include <cstdint>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/power_monitor.h"
#include "v2/hal/supply_voltage_source.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class SensorTask : public App::StageScopedTask, public App::UsePublisher<SensorTask> {
    private:
        PowerMonitor& m_monitor;
        SupplyVoltageSource& m_supply;

    public:
        static constexpr uint32_t PERIOD_MS = 33;

        SensorTask(PowerMonitor& monitor, SupplyVoltageSource& supply)
            : App::StageScopedTask(
                  PERIOD_MS,
                  App::StageMask::of<NormalStage, EnergyStage, ProfilePickerStage>()
              ),
              m_monitor(monitor), m_supply(supply) {}

        void on_tick(uint32_t now_ms) override {
            const auto load = m_monitor.read();
            const auto supply = m_supply.read();
            if (!load.valid && !supply.valid) {
                return;
            }
            publish(SensorEvent{
                LoadReading{now_ms, load.mv, load.ma},
                SupplyReading{now_ms, supply.mv, supply.valid},
            });
        }
    };

} // namespace pocketpd
