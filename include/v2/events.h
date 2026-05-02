/**
 * @file events.h
 * @brief PocketPD v2 application event variant.
 */
#pragma once

#include <tempo/bus/event.h>

#include <cstdint>

#include "v2/state.h"

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
    };

    /**
     * @brief Published by EncoderTask when the encoder position has changed.
     * `delta` is the signed tick count since the previous sample.
     */
    struct EncoderEvent {
        int16_t delta = 0;
    };

    using Event = tempo::Events<PdReadyEvent, ButtonEvent, EncoderEvent>;

} // namespace pocketpd
