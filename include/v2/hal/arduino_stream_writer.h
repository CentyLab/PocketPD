/**
 * @file arduino_stream_writer.h
 * @brief `tempo::StreamWriter` backed by the global Arduino `Serial` UART.
 */
#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <tempo/hardware/stream.h>

namespace pocketpd {

    class ArduinoStreamWriter : public tempo::StreamWriter {
    public:
        size_t write(const char* data, size_t length) override {
            return Serial.write(data, length);
        }
    };

} // namespace pocketpd

#endif // ARDUINO
