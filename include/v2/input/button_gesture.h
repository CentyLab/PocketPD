/**
 * @file button_gesture.h
 * @brief Pure state machine that turns a (held, now_ms) sample stream into SHORT / LONG gestures.
 * Owned per-button by ButtonTask. No publisher, no hardware — fully unit testable.
 */
#pragma once

#include <tempo/core/time.h>

#include <cstdint>
#include <optional>

#include "v2/state.h"

namespace pocketpd {

    struct ButtonGestureConfig {
        uint32_t long_press_ms = 1500;
    };

    class ButtonGestureDetector {
    private:
        enum class State : uint8_t {
            IDLE,
            PRESSED,  
            POST_LONG,
        };
        
        State m_state = State::IDLE;

        ButtonGestureConfig m_config;
        tempo::TimeoutTimer m_long_timeout;

    public:
        explicit ButtonGestureDetector(ButtonGestureConfig config = {}) : m_config(config) {}

        /**
         * @brief Feed one sample. Returns a gesture on the tick it is recognized, otherwise
         * `std::nullopt`.
         *
         * SHORT fires on release if the press did not cross the long-press threshold. LONG fires
         * once at the threshold while still held; the eventual release produces no event.
         *
         * State diagram:
         * @verbatim
         *     ┌──────┐  hold / arm timer   ┌─────────┐  reached / LONG    ┌───────────┐
         *     │ IDLE │ ───────────────────▶│ PRESSED │ ──────────────────▶│ POST_LONG │
         *     └──────┘                     └────┬────┘                    └─────┬─────┘
         *         ▲                             │                               │
         *         │       release / SHORT       │            release            │
         *         └─────────────────────────────┴───────────────────────────────┘
         * @endverbatim
         */
        std::optional<Gesture> update(bool is_holding, uint32_t now_ms) {
            switch (m_state) {
            case State::IDLE:
                if (is_holding) {
                    m_state = State::PRESSED;
                    m_long_timeout.set(now_ms, m_config.long_press_ms);
                }

                return std::nullopt;

            case State::PRESSED:
                if (!is_holding) {
                    m_state = State::IDLE;
                    m_long_timeout.disarm();
                    return Gesture::SHORT;
                }

                if (m_long_timeout.reached(now_ms)) {
                    m_state = State::POST_LONG;
                    m_long_timeout.disarm();
                    return Gesture::LONG;
                }

                return std::nullopt;

            case State::POST_LONG:
                if (!is_holding) {
                    m_state = State::IDLE;
                }

                return std::nullopt;
            }

            return std::nullopt;
        }
    };

} // namespace pocketpd
