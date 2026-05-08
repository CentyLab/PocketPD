/**
 * @file ina226_power_monitor.h
 * @brief INA226-backed implementation of `PowerMonitor`.
 *
 * Borrows an `INA226` driver instance by reference. The driver's I2C bus
 * setup and calibration happen in `begin()`.
 */
#pragma once

#include <INA226.h>

#include "v2/hal/power_monitor.h"

namespace pocketpd {

    class Ina226PowerMonitor : public PowerMonitor {
    private:
        INA226& m_driver;

    public:
        explicit Ina226PowerMonitor(INA226& driver) : m_driver(driver) {}

        void begin() override {
            m_driver.begin();
            m_driver.setMaxCurrentShunt(20.0f, 0.005f);
            m_driver.setAverage(INA226_4_SAMPLES);
        }

        PowerReading read() override {
            const auto mv = static_cast<uint32_t>(m_driver.getBusVoltage_mV());
            const auto ma = static_cast<uint32_t>(m_driver.getCurrent_mA());
            const bool ok = m_driver.getLastError() == 0;
            return PowerReading{mv, ma, ok};
        }

        /**
         * @brief Stub for current breaker feature. 
         * Ticket: Issue #42 — not yet completed
         * 
         */
        void set_alert_threshold_ma(uint32_t /*ma*/) override {

        }
    };

} // namespace pocketpd
