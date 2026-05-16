/**
 * @file supply_voltage_source.h
 * @brief Abstract source for the supply-side voltage.
 *
 * HW1.3+ reads through the V_SENSE ADC divider. Earlier boards query the
 * AP33772 VOLTAGE register through `PdSinkController`.
 */
#pragma once

#include <cstdint>

namespace pocketpd {

    /**
     * @brief One supply-voltage sample. `valid` false on hardware read error.
     */
    struct SupplyVoltageReading {
        uint32_t mv = 0;
        bool valid = false;
    };

    class SupplyVoltageSource {
    public:
        virtual ~SupplyVoltageSource() = default;

        virtual void begin() = 0;

        /** @brief Sample the supply voltage. */
        virtual SupplyVoltageReading read() = 0;
    };

} // namespace pocketpd
