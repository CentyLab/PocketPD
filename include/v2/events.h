/**
 * @file events.h
 * @brief PocketPD v2 application event variant.
 */
#pragma once

#include <cstdint>

#include <tempo/bus/event.h>

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

    using Event = tempo::Events<PdReadyEvent>;

} // namespace pocketpd
