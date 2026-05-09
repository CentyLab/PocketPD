/**
 * @file state.h
 * @brief Shared input enums + sensor reading struct.
 */
#pragma once

#include <cstdint>

namespace pocketpd {

    /**
     * @brief Identifier for the three physical buttons. `L` and `R` denote
     * the left and right side buttons; `ENCODER` is the rotary push.
     */
    enum class ButtonId : uint8_t {
        ENCODER,
        R,
        L,
    };

    enum class Gesture : uint8_t {
        SHORT,
        LONG,
    };

    /**
     * @brief Latest sensor reading from INA226.
     */
    struct SensorSnapshot {
        uint32_t timestamp_ms = 0;
        uint32_t vbus_mv = 0;
        uint32_t current_ma = 0;
    };

} // namespace pocketpd
