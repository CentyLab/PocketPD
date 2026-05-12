/**
 * @file pocketpd.h
 * @brief PocketPD v2 product-level types and constants.
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
    constexpr std::array<uint16_t, 3> VOLTAGE_INCREMENTS_MV = {20, 100, 1000};
    constexpr std::array<uint16_t, 3> CURRENT_INCREMENTS_MA = {50, 100, 1000};

    // PPS RDO stepping per USB-PD 3.0 spec.
    constexpr int32_t PPS_VOLTAGE_STEP_MV = 20;
    constexpr int32_t PPS_CURRENT_STEP_MA = 50;

    // —— I2C addresses

    constexpr uint8_t INA226_I2C_ADDR = 0x40;

    /**
     * @brief EEPROM autosave debounce window in ms after the user stops adjusting. Force-save
     * bypasses the debounce.
     */
    constexpr uint32_t EEPROM_SAVE_DEBOUNCE_MS = 2000;

} // namespace pocketpd
