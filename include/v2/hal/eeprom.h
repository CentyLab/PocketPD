/**
 * @file eeprom.h
 * @brief Persistent preferences HAL: Preferences POD, codec, and abstract interface.
 *
 * Wire layout at storage offset 0:
 *   [0]            uint8_t  version  (= PREFERENCES_LAYOUT_VERSION)
 *   [1 .. N]       payload  (sizeof(Preferences) bytes)
 *   [N+1]          uint8_t  crc8     (over version byte + payload)
 *
 * Codec helpers (`encode_preferences`, `decode_preferences`) are free functions so they
 * are testable.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketpd {

    struct Preferences {
        bool skip_picker_on_boot = false;
        bool voltage_comp_enabled = false;
        bool flip_display = false;
    };

    inline bool operator==(const Preferences& a, const Preferences& b) {
        return a.skip_picker_on_boot == b.skip_picker_on_boot &&
               a.voltage_comp_enabled == b.voltage_comp_enabled &&
               a.flip_display == b.flip_display;
    }

    inline bool operator!=(const Preferences& a, const Preferences& b) {
        return !(a == b);
    }

    static constexpr uint8_t PREFERENCES_LAYOUT_VERSION = 3;
    static constexpr size_t SIZE = sizeof(Preferences);
    static constexpr size_t EEPROM_PREFERENCES_BYTES = 1 + SIZE + 1;

    inline uint8_t crc8(const uint8_t* data, size_t len) {
        uint8_t crc = 0;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b) {
                crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                                   : static_cast<uint8_t>(crc << 1);
            }
        }
        return crc;
    }


    inline void encode_preferences(const Preferences& payload, uint8_t* out) {
        out[0] = PREFERENCES_LAYOUT_VERSION;
        std::memcpy(out + 1, &payload, SIZE);
        out[1 + SIZE] = crc8(out, 1 + SIZE);
    }

    inline bool decode_preferences(const uint8_t* payload, Preferences& out) {
        if (payload[0] != PREFERENCES_LAYOUT_VERSION) {
            return false;
        }

        const uint8_t expected = crc8(payload, 1 + SIZE);
        if (expected != payload[1 + SIZE]) {
            return false;
        }

        std::memcpy(&out, payload + 1, SIZE);
        return true;
    }

    class Eeprom {
    public:
        virtual ~Eeprom() = default;

        /**
         * @brief Read persisted bytes and decode into `out`.
         * @return true on success; false if version/CRC mismatch (out left at defaults).
         */
        virtual bool load(Preferences& out) = 0;

        /**
         * @brief Encode `in` and persist. Implementations are free to skip flash writes
         * when bytes already match.
         */
        virtual bool save(const Preferences& in) = 0;
    };

} // namespace pocketpd
