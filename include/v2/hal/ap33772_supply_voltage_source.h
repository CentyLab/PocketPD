/**
 * @file ap33772_supply_voltage_source.h
 * @brief `SupplyVoltageSource` backed by the AP33772 VOLTAGE register, for
 * boards without the V_SENSE ADC divider (HW < 1.3).
 */
#pragma once

#include "v2/hal/pd_sink_controller.h"
#include "v2/hal/supply_voltage_source.h"

namespace pocketpd {

    class Ap33772SupplyVoltageSource : public SupplyVoltageSource {
    private:
        PdSinkController& m_sink;

    public:
        explicit Ap33772SupplyVoltageSource(PdSinkController& sink) : m_sink(sink) {}

        void begin() override {}

        SupplyVoltageReading read() override {
            const int mv = m_sink.read_vbus_mv();
            if (mv < 0) {
                return SupplyVoltageReading{0, false};
            }
            return SupplyVoltageReading{static_cast<uint32_t>(mv), true};
        }
    };

} // namespace pocketpd
