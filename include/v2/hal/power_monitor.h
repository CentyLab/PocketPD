/**
 * @file power_monitor.h
 * @brief Bus voltage / current sensor HAL.
 *
 * Stages, tasks, and tests depend only on the `PowerMonitor` interface. The
 * Arduino-backed implementation (`Ina226PowerMonitor`) is gated behind
 * `#ifdef ARDUINO` so the interface compiles cleanly on the `native` env
 * without pulling `Wire.h` through the INA226 driver. Mocks live in
 * `test/mocks/MockPowerMonitor.h`.
 */
#pragma once

#include <cstdint>

#ifdef ARDUINO
#include <INA226.h>
#endif

namespace pocketpd {

    /**
     * @brief One sampled bus reading. `valid` is false if the underlying driver
     * could not produce a fresh sample (e.g. I2C error).
     */
    struct PowerReading {
        uint32_t mv = 0;
        uint32_t ma = 0;
        bool valid = false;
    };

    /**
     * @brief Abstract bus voltage / current monitor. Surface today covers a
     * blocking sample plus an alert-threshold stub reserved for issue #42
     * (INA226 Alert pin breaker).
     */
    class PowerMonitor {
    public:
        virtual ~PowerMonitor() = default;

        /** @brief Bring up the underlying driver (I2C config, calibration, ...). */
        virtual void begin() = 0;

        /** @brief Sample bus voltage and current. */
        virtual PowerReading read() = 0;

        /**
         * @brief Configure overcurrent alert threshold in mA.
         *
         * Stub for issue #42 (INA226 Alert pin breaker). Implementations that do
         * not yet wire the Alert pin should treat this as a no-op.
         */
        virtual void set_alert_threshold_ma(uint32_t ma) = 0;
    };

#ifdef ARDUINO

    /**
     * @brief INA226-backed implementation of `PowerMonitor`.
     *
     * Borrows an `INA226` driver instance by reference. The driver's I2C bus
     * setup and calibration happen in `begin()`.
     */
    class Ina226PowerMonitor : public PowerMonitor {
    private:
        INA226& m_driver;

    public:
        explicit Ina226PowerMonitor(INA226& driver) : m_driver(driver) {}

        void begin() override {
            m_driver.begin();
        }

        PowerReading read() override {
            const auto mv = static_cast<uint32_t>(m_driver.getBusVoltage_mV());
            const auto ma = static_cast<uint32_t>(m_driver.getCurrent_mA());
            const bool ok = m_driver.getLastError() == 0;
            return PowerReading{mv, ma, ok};
        }

        void set_alert_threshold_ma(uint32_t /*ma*/) override {
            // issue #42 — Alert pin breaker not yet wired
        }
    };

#endif // ARDUINO

} // namespace pocketpd
