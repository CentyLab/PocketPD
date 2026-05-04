/**
 * @file rotary_encoder_input.h
 * @brief `RotaryEncoder`-backed implementation of `EncoderInput`.
 *
 * Owns a `RotaryEncoder` instance. The pin ISR uses a static instance pointer
 * because `attachInterrupt` only takes free functions; only one `RotaryEncoderInput` may be active
 * at a time.
 */
#pragma once

#include <Arduino.h>
#include <RotaryEncoder.h>
#include <tempo/hardware/encoder_input.h>

#include <cstdint>

namespace pocketpd {

    class RotaryEncoderInput : public tempo::EncoderInput {
    private:
        static inline RotaryEncoderInput* s_instance = nullptr;
        mutable RotaryEncoder m_encoder;
        const int m_pin_a;
        const int m_pin_b;

        static void isr_handler() {
            if (s_instance != nullptr) {
                s_instance->m_encoder.tick();
            }
        }

    public:
        RotaryEncoderInput(int pin_a, int pin_b)
            : m_encoder(pin_b, pin_a, RotaryEncoder::LatchMode::FOUR3),
              m_pin_a(pin_a),
              m_pin_b(pin_b) {

            s_instance = this;
        }

        /// Wire the pin ISRs
        void begin() {
            attachInterrupt(digitalPinToInterrupt(m_pin_a), isr_handler, CHANGE);
            attachInterrupt(digitalPinToInterrupt(m_pin_b), isr_handler, CHANGE);
        }

        int32_t position() const override {
            return static_cast<int32_t>(m_encoder.getPosition());
        }
    };

} // namespace pocketpd
