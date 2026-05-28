/**
 * @file arduino_eeprom.h
 * @brief Arduino `EEPROM` (flash-emulated on RP2040) implementation of `Eeprom`.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <EEPROM.h>

#include "v2/hal/eeprom.h"

namespace pocketpd {

    class ArduinoEeprom : public Eeprom {
    private:
        bool m_began = false;

    public:
        static constexpr size_t MAX_SIZE = 4096;
        
        /**
         * @brief Reserve `bytes` of emulated EEPROM. Must be called from `setup()`
         * before any load/save. Idempotent.
         */
        void begin(size_t bytes = MAX_SIZE) {
            if (m_began) {
                return;
            }

            EEPROM.begin(bytes);
            m_began = true;
        }

        bool load(Preferences& out) override {
            std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};
            for (size_t i = 0; i < buf.size(); ++i) {
                buf[i] = EEPROM.read(static_cast<int>(i));
            }

            return decode_preferences(buf.data(), out);
        }

        bool save(const Preferences& in) override {
            std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};
            encode_preferences(in, buf.data());

            bool changed = false;
            for (size_t i = 0; i < buf.size(); ++i) {
                const uint8_t cur = EEPROM.read(static_cast<int>(i));
                if (cur != buf[i]) {
                    EEPROM.write(static_cast<int>(i), buf[i]);
                    changed = true;
                }
            }

            if (changed) {
                return EEPROM.commit();
            }

            return true;
        }
    };

} // namespace pocketpd
