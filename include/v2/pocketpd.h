/**
 * @file pocketpd.h
 * @brief PocketPD v2 product-level types, configuration constants, etc.
 *
 * Header is kept dependency-free so it can be included from hardware adapters, stages, tasks, and
 * native tests alike.
 */
#pragma once

#include <array>
#include <cstdint>

namespace pocketpd {

    // —— Firmware identity

    constexpr const char* FW_VERSION = VERSION;

    /**
     * @brief Stage timing thresholds in milliseconds.
     *
     */
    constexpr uint32_t BOOT_TO_OBTAIN_MS = 500;
    constexpr uint32_t OBTAIN_TO_PROFILE_PICKER_MS = 1500;
    constexpr uint32_t PICKER_PASSTHROUGH_AUTO_MS = 3000;

    /**
     * @brief What the rotary encoder is currently editing.
     */
    enum class AdjustMode : uint8_t {
        VOLTAGE,
        CURRENT,
    };

    /**
     * @brief Encoder increment tables (mV / mA).
     *
     * Short-press encoder cycles the active index. Mirrors v1
     * voltageIncrement[3] / currentIncrement[3] in StateMachine.
     */
    constexpr std::array<uint16_t, 3> VOLTAGE_INCREMENTS_MV = {1000, 100, 20};
    constexpr std::array<uint16_t, 3> CURRENT_INCREMENTS_MA = {1000, 100, 50};

    // PPS RDO stepping per USB-PD 3.0 spec.
    constexpr int32_t PPS_VOLTAGE_STEP_MV = 20;
    constexpr int32_t PPS_CURRENT_STEP_MA = 50;
    
    constexpr uint8_t pin_encoder_SW = 18;
    constexpr uint8_t pin_encoder_A = 19;  // CLK
    constexpr uint8_t pin_encoder_B = 20;  // DATA

    constexpr uint8_t pin_output_Enable = 1;
    constexpr uint8_t pin_button_outputSW = 10;
    constexpr uint8_t pin_button_selectVI = 11;

    constexpr uint8_t pin_SDA = 4;
    constexpr uint8_t pin_SCL = 5;

    constexpr uint8_t pin_VSENSE = 29;  // ADC3 — V_SENSE divider (2/13 of VBUS)

    // —— I2C addresses

    constexpr uint8_t INA226_I2C_ADDR = 0x40;

    /**
     * @brief EEPROM autosave debounce window in ms after the user stops adjusting. Force-save
     * bypasses the debounce.
     */
    constexpr uint32_t EEPROM_SAVE_DEBOUNCE_MS = 2000;

    // —— Input types

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

} // namespace pocketpd
