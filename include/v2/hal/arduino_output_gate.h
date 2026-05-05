/**
 * @file arduino_output_gate.h
 * @brief Arduino-backed `OutputGate` implementation that drives a single GPIO pin.
 *
 * Active-high wiring on the PocketPD board: `HIGH` = output on, `LOW` = output off. `begin()`
 * forces the pin LOW so power stays off until something explicitly enables it.
 */
#pragma once

#include <Arduino.h>

#include <cstdint>

#include "v2/hal/output_gate.h"

namespace pocketpd {

    class ArduinoOutputGate : public OutputGate {
    private:
        const uint8_t m_pin;
        bool m_enabled = false;

    public:
        explicit ArduinoOutputGate(uint8_t pin) : m_pin(pin) {}

        void begin() {
            pinMode(m_pin, OUTPUT);
            digitalWrite(m_pin, LOW);
            m_enabled = false;
        }

        void enable() override {
            digitalWrite(m_pin, HIGH);
            m_enabled = true;
        }

        void disable() override {
            digitalWrite(m_pin, LOW);
            m_enabled = false;
        }

        bool is_enabled() const override {
            return m_enabled;
        }
    };

} // namespace pocketpd
