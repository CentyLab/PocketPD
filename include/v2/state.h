/**
 * @file state.h
 * @brief Shared runtime state structs and input enums.
 *
 * One `SensorSnapshot` and one `UiState` instance lives in main.cpp; both are
 * passed by reference into every Stage and Task. Stages may mutate UiState;
 * SensorTask is the only writer for SensorSnapshot. No globals.
 *
 */
#pragma once

#include <cstdint>

#include "v2/pocketpd.h"

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
     *
     * Updated by SensorTask at 33 ms period inside Normal stages. Read-only
     * for everything else. `valid` is false until the first successful read.
     */
    struct SensorSnapshot {
        uint32_t timestamp_ms = 0;
        uint32_t vbus_mv = 0;
        uint32_t current_ma = 0;
        bool valid = false;
    };

    /**
     * @brief Persistent UI / app state across stage transitions.
     *
     * Lifetime equals the lifetime of the app. EEPROM mirrors a subset
     * (target_mv, target_ma, active_pdo_index).
     */
    struct UiState {
        // —— PD request

        int32_t target_mv = 5000; // issue #33: deterministic boot default
        int32_t target_ma = 1000;
        uint8_t active_pdo_index = 0;

        // —— Encoder edit

        uint8_t voltage_idx = 0;
        uint8_t current_idx = 0;
        AdjustMode adjust_mode = AdjustMode::VOLTAGE;

        // —— Output

        bool output_on = false;
        OutputScreen screen = OutputScreen::NORMAL;

        // —— Animation (driven by BlinkTask at 500 ms)

        bool cursor_visible = true;
        uint8_t arrow_frame = 0;

        // —— Autosave bookkeeping (Normal stages only)

        uint32_t last_edit_ms = 0;
        bool dirty = false;
        bool force_save = false;

        // —— Energy accumulator (Normal stages, output enabled)

        double accumulated_wh = 0.0;
        double accumulated_ah = 0.0;
        uint32_t output_session_start_ms = 0;
        uint32_t historical_output_seconds = 0;

        // —— Lock

        bool input_locked = false; // issue #36 hook; always false in parity port
    };

} // namespace pocketpd
