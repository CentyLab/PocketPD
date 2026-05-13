/**
 * @file two_buttons_gesture.h
 * @brief Pure state machine that recognizes a two-button long-press sample stream.
 */
#pragma once

#include <tempo/core/time.h>

#include <cstdint>
#include <optional>

#include "v2/input/button_gesture.h"
#include "v2/state.h"

namespace pocketpd {

    class TwoButtonsGestureDetector {
    private:
        enum class TwoButtonsState : uint8_t {
            IDLE,
            ARMED,
            LATCHED,
        };

        TwoButtonsState m_state = TwoButtonsState::IDLE;
        ButtonGestureConfig m_config;
        tempo::TimeoutTimer m_long_timeout;

    public:
        explicit TwoButtonsGestureDetector(ButtonGestureConfig config = {}) : m_config(config) {}

        /** @brief True while a two-button gesture is in progress (ARMED) or holding suppression
         * (LATCHED).
         */
        bool is_active() const {
            return m_state != TwoButtonsState::IDLE;
        }

        /**
         * @brief Feed one sample of each button's holding state. Returns `Gesture::LONG` on the
         * tick the two-button gesture fires, otherwise `std::nullopt`.
         *
         * LONG fires once at the threshold while both are still held. Releasing either button
         * before the threshold aborts without emit. Release wins ties against timer expiry on
         * the same tick. LATCHED holds until BOTH release, so callers use `is_active()` as the
         * suppression predicate for individual L/R gestures.
         *
         * State diagram:
         * @verbatim
         *    ┌──────┐ both held / arm   ┌───────┐  timer / LONG  ┌─────────┐
         *    │ IDLE │ ─────────────────▶│ ARMED │ ──────────────▶│ LATCHED │
         *    └──────┘                   └───────┘                └────┬────┘
         *        ▲                                                    │
         *        │                                                    │
         *        │                   both released                    │
         *        └────────────────────────────────────────────────────┘
         *
         *    ARMED + one released ─▶ LATCHED (abort, no emit).
         * @endverbatim
         */
        std::optional<Gesture> update(uint32_t now_ms, bool is_holding_l, bool is_holding_r) {
            const bool both_holding = is_holding_l && is_holding_r;
            const bool none_holding = !is_holding_l && !is_holding_r;

            switch (m_state) {
            case TwoButtonsState::IDLE:
                if (both_holding) {
                    m_state = TwoButtonsState::ARMED;
                    m_long_timeout.set(now_ms, m_config.long_press_ms);
                }

                return std::nullopt;
            case TwoButtonsState::ARMED:
                if (!both_holding) {
                    m_state = TwoButtonsState::LATCHED;
                    m_long_timeout.disarm();
                    return std::nullopt;
                }

                if (m_long_timeout.reached(now_ms)) {
                    m_state = TwoButtonsState::LATCHED;
                    m_long_timeout.disarm();
                    return Gesture::LONG;
                }

                return std::nullopt;
            case TwoButtonsState::LATCHED:
                if (none_holding) {
                    m_state = TwoButtonsState::IDLE;
                }

                return std::nullopt;
            }

            return std::nullopt;
        }
    };

} // namespace pocketpd
