/**
 * @file adc_supply_voltage_source.h
 * @brief ADC-backed `SupplyVoltageSource` for HW1.3+ (V_SENSE divider into ADC3).
 */
#pragma once

#include <Arduino.h>

#include <cstdint>

#include "v2/hal/supply_voltage_source.h"

namespace pocketpd {

    class AdcSupplyVoltageSource : public SupplyVoltageSource {
    private:
        uint8_t m_pin;

        // RP2040 ADC: 12-bit, 3.3 V reference. V_SENSE divider = 2/13 of VBUS.
        static constexpr uint32_t ADC_REF_MV = 3300;
        static constexpr uint16_t ADC_MAX = 4095;
        static constexpr uint32_t DIVIDER_NUM = 13;
        static constexpr uint32_t DIVIDER_DEN = 2;

    public:
        explicit AdcSupplyVoltageSource(uint8_t pin) : m_pin(pin) {}

        void begin() override {
            analogReadResolution(12);
        }

        SupplyVoltageReading read() override {
            const uint32_t raw = analogRead(m_pin);
            const uint32_t adc_mv = (raw * ADC_REF_MV) / ADC_MAX;
            const uint32_t vbus_mv = (adc_mv * DIVIDER_NUM) / DIVIDER_DEN;
            return SupplyVoltageReading{vbus_mv, true};
        }
    };

} // namespace pocketpd
