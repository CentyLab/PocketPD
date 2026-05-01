/**
 * @file arduino_clock.h
 * @brief `tempo::Clock` backed by Arduino's `millis()`.
 */
#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <tempo/core/time.h>

namespace pocketpd {

    class ArduinoClock : public tempo::Clock {
    public:
        uint32_t now_ms() const override {
            return ::millis();
        }
    };

} // namespace pocketpd

#endif // ARDUINO
