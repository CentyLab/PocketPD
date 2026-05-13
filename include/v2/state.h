/**
 * @file state.h
 * @brief Shared input enums + sensor reading struct.
 */
#pragma once

#include <cstdint>

namespace pocketpd {

    /**
     * @brief Identifier for the three physical buttons plus a synthetic L_R combo.
     * `L` and `R` denote the left and right side buttons; `ENCODER` is the rotary push.
     */
    enum class ButtonId : uint8_t {
        ENCODER,
        R,
        L,
        L_R,
    };

    inline const char* to_string(ButtonId btn_id) {
        switch (btn_id) {
        case ButtonId::ENCODER:
            return "ENCODER_BTN";
        case ButtonId::L:
            return "L_BTN";
        case ButtonId::R:
            return "R_BTN";
        case ButtonId::L_R:
            return "L_R_SYNTHETIC";
        default:
            return "UNKNOWN";
        }
    }

    enum class Gesture : uint8_t {
        SHORT,
        LONG,
    };

    inline const char* to_string(Gesture g) {
        switch (g) {
        case Gesture::SHORT:
            return "SHORT";
        case Gesture::LONG:
            return "LONG";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * @brief Latest sensor reading from INA226.
     */
    struct SensorSnapshot {
        uint32_t timestamp_ms = 0;
        uint32_t vbus_mv = 0;
        uint32_t current_ma = 0;
    };

} // namespace pocketpd
