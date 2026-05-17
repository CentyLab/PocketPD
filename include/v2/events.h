/**
 * @file events.h
 * @brief PocketPD v2 application event variant.
 */
#pragma once

#include <cstdint>

#include <tempo/bus/event.h>

#include "v2/pocketpd.h"
#include "v2/util/filter.h"

namespace pocketpd {

    /**
     * @brief Published once by ObtainStage after a successful PD negotiation.
     *
     * Carries enough info for downstream stages to decide whether the charger
     * advertises any PPS APDOs (issue #24 multi-PPS) without re-querying the
     * sink. Detailed PDO inspection still goes through `PdSinkController`.
     */
    struct PdReadyEvent {
        uint8_t num_pdo = 0;
        uint8_t pps_count = 0;
    };

    /**
     * @brief Published by ButtonTask on each detected gesture.
     *
     * SHORT fires once on release if the press was shorter than the long-press
     * threshold; LONG fires once when the press duration crosses the threshold
     * while still pressed (no release event in that case).
     */
    struct ButtonEvent {
        ButtonId id = ButtonId::ENCODER;
        Gesture gesture = Gesture::SHORT;

        bool r_short() const {
            return id == ButtonId::R && gesture == Gesture::SHORT;
        }

        bool r_long() const {
            return id == ButtonId::R && gesture == Gesture::LONG;
        }

        bool l_long() const {
            return id == ButtonId::L && gesture == Gesture::LONG;
        }

        bool l_short() const {
            return id == ButtonId::L && gesture == Gesture::SHORT;
        }

        bool lr_long() const {
            return id == ButtonId::L_R && gesture == Gesture::LONG;
        }
    };

    /**
     * @brief Published by EncoderTask when the encoder position has changed.
     * `delta` is the signed tick count since the previous sample.
     */
    struct EncoderEvent {
        int delta = 0;
    };

    /**
     * @brief Load-side reading from INA226.
     */
    struct LoadReading {
        static constexpr uint32_t LOAD_EMA_DEN = 4;
        static constexpr uint32_t SNAP_MV = 200;
        static constexpr uint32_t SNAP_MA = 20;

        uint32_t timestamp_ms = 0;
        uint32_t vbus_mv = 0;
        uint32_t current_ma = 0;

        LoadReading ema(const LoadReading& sample ) const {
            return {
                .timestamp_ms = sample.timestamp_ms,
                .vbus_mv = Filter::ema(vbus_mv, sample.vbus_mv, LOAD_EMA_DEN, SNAP_MV),
                .current_ma = Filter::ema(current_ma, sample.current_ma, LOAD_EMA_DEN, SNAP_MA),
            };
        }
    };

    /**
     * @brief Supply-side reading  Sourced from the V_SENSE ADC divider on HW1.3+ or the AP33772
     * VOLTAGE register on earlier board.
     */
    struct SupplyReading {
        static constexpr uint32_t SUPPLY_EMA_DEN = 8;
        static constexpr uint32_t SNAP_MV = 200;

        uint32_t timestamp_ms = 0;
        uint32_t mv = 0;
        bool valid = false;

        SupplyReading ema(const SupplyReading& sample) const {
            return {
                .timestamp_ms = sample.timestamp_ms,
                .mv = Filter::ema(mv, sample.mv, SUPPLY_EMA_DEN, SNAP_MV),
                .valid = sample.valid,
            };
        }
    };

    /**
     * @brief Published by SensorTask
     */
    struct SensorEvent {
        LoadReading load;
        SupplyReading supply;
    };

    /**
     * @brief Published by EnergyTask
     */
    struct EnergyEvent {
        double accumulated_wh = 0.0;
        double accumulated_ah = 0.0;
        uint32_t total_seconds = 0;
    };

    using Event = tempo::Events<PdReadyEvent, ButtonEvent, EncoderEvent, SensorEvent, EnergyEvent>;

} // namespace pocketpd
