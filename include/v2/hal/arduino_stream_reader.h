/**
 * @file arduino_stream_reader.h
 * @brief `tempo::StreamReader` backed by the global Arduino `Serial` UART.
 */
#pragma once

#include <Arduino.h>
#include <tempo/hardware/stream.h>

namespace pocketpd {

    class ArduinoStreamReader : public tempo::StreamReader {
    public:
        int available() override {
            return Serial.available();
        }

        int read() override {
            return Serial.read();
        }
    };

} // namespace pocketpd
