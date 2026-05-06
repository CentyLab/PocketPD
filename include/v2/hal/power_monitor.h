/**
 * @file power_monitor.h
 * @brief Abstract bus voltage / current sensor HAL.
 *
 * The INA226-backed implementation lives in `ina226_power_monitor.h`
 */
#pragma once

#include <cstdint>

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
     * @brief Abstract bus voltage / current monitor.
     */
    class PowerMonitor {
    public:
        virtual ~PowerMonitor() = default;

        virtual void begin() = 0;

        /**
         * @brief Sample bus voltage and current.
         */
        virtual PowerReading read() = 0;

        /**
         * @brief Configure overcurrent alert threshold in mA.
         *
         * Stub for issue #42 (INA226 Alert pin breaker).
         */
        virtual void set_alert_threshold_ma(uint32_t ma) = 0;
    };

} // namespace pocketpd
