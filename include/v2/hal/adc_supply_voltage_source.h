/**
 * @file adc_supply_voltage_source.h
 * @brief ADC-backed `SupplyVoltageSource` for HW1.3+ (V_SENSE divider into ADC3).
 */
#pragma once

#include <cstdint>

#include <Arduino.h>

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

        // Oversample N raw samples per read(); sqrt(N) noise reduction on uncorrelated ADC noise.
        static constexpr uint16_t OVERSAMPLE_N = 64; // tested ~300us loop time at 64 samples

    public:
        explicit AdcSupplyVoltageSource(uint8_t pin) : m_pin(pin) {}

        void begin() override {
            analogReadResolution(12);
        }

        SupplyVoltageReading read() override {
            uint32_t sum = 0;
            for (int i = 0; i < OVERSAMPLE_N; i++) {
                sum += analogRead(m_pin);
            }

            const uint64_t numerator = (uint64_t) sum * ADC_REF_MV * DIVIDER_NUM;
            const uint64_t denominator = (uint64_t) OVERSAMPLE_N * ADC_MAX * DIVIDER_DEN;
            const uint32_t vbus_mv = numerator / denominator;

            return SupplyVoltageReading{vbus_mv, true};
        }
    };

} // namespace pocketpd
