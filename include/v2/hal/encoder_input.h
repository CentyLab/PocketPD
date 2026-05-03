/**
 * @file encoder_input.h
 * @brief Pure-virtual rotary encoder HAL. Arduino impl wraps `RotaryEncoder`; pin ISR uses a
 * static instance pointer because `attachInterrupt` only takes free functions.
 */
#pragma once

#include <cstdint>

namespace pocketpd {

    class EncoderInput {
    public:
        virtual ~EncoderInput() = default;

        /// Accumulated tick position, signed.
        virtual int32_t position() const = 0;
    };

} // namespace pocketpd

#ifdef ARDUINO
#include <Arduino.h>
#include <RotaryEncoder.h>

namespace pocketpd {

    class RotaryEncoderInput : public EncoderInput {
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

        /// Wire the pin ISRs. Call once from `setup()`. Only one instance may be active.
        void begin() {
            attachInterrupt(digitalPinToInterrupt(m_pin_a), isr_handler, CHANGE);
            attachInterrupt(digitalPinToInterrupt(m_pin_b), isr_handler, CHANGE);
        }

        int32_t position() const override {
            return static_cast<int32_t>(m_encoder.getPosition());
        }
    };

} // namespace pocketpd
#endif
