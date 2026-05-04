/**
 * @file ez_button_input.h
 * @brief `ezButton`-backed implementation of `ButtonInput`.
 *
 * Owns an `ezButton` instance and forwards `ButtonInput` calls to it. Active-low wiring
 * (held = `LOW`) matches the PocketPD board.
 */
#pragma once

#include <Arduino.h>
#include <ezButton.h>
#include <tempo/hardware/button_input.h>

namespace pocketpd {

    class EzButtonInput : public tempo::ButtonInput {
    private:
        mutable ezButton m_button;

    public:
        explicit EzButtonInput(int pin, unsigned long debounce_ms = 50) : m_button(pin) {
            m_button.setDebounceTime(debounce_ms);
        }

        void update() override {
            m_button.loop();
        }

        bool is_held() const override {
            return m_button.getState() == LOW;
        }
    };

} // namespace pocketpd
