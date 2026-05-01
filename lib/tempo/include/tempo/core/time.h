#pragma once

#include <cstdint>

namespace tempo {

    /**
     * @brief Wraparound-safe comparison helpers for 32-bit timestamps.
     *
     */
    inline int32_t time_diff(uint32_t now, uint32_t deadline) {
        return static_cast<int32_t>(now - deadline);
    }

    inline bool time_reached(uint32_t now, uint32_t deadline) {
        return time_diff(now, deadline) >= 0;
    }

    /**
     * @brief Implementation-agnostic Clock interface
     *
     */
    class Clock {
    public:
        virtual ~Clock() = default;
        virtual uint32_t now_ms() const = 0;
    };

    /**
     * @brief Lightweight periodic gate for use inside a Stage's `on_tick` (or any caller that ticks
     * faster than the desired cadence).
     *
     * Wraparound-safe: uses `time_reached` for all comparisons, so a 32-bit `now_ms` rollover is
     * handled correctly.
     *
     * Example:
     * @code
     *   class MyStage : public App::Stage {
     *       tempo::IntervalTimer m_heartbeat{500};
     *
     *       void on_tick(Conductor&, uint32_t now_ms) override {
     *           if (m_heartbeat.tick(now_ms)) {
     *               log.info("alive");
     *           }
     *       }
     *   };
     * @endcode
     */
    class IntervalTimer {
    private:
        uint32_t m_period_ms;
        uint32_t m_due_ms = 0;
        bool m_armed = false;

    public:
        explicit IntervalTimer(uint32_t period_ms) : m_period_ms(period_ms) {}

        /**
         * @brief Returns true once when the period elapses, false otherwise.
         *
         * The first call arms the timer relative to `now_ms` and returns false.
         */
        bool tick(uint32_t now_ms) {
            if (!m_armed) {
                m_due_ms = now_ms + m_period_ms;
                m_armed = true;
                return false;
            }
            if (time_reached(now_ms, m_due_ms)) {
                m_due_ms = now_ms + m_period_ms;
                return true;
            }
            return false;
        }

        /**
         * @brief Re-arm relative to `now_ms`. Next fire is one period out.
         *
         * Use after handling a stage transition or any condition that should suppress the next
         * scheduled fire.
         */
        void reset(uint32_t now_ms) {
            m_due_ms = now_ms + m_period_ms;
            m_armed = true;
        }

        uint32_t period_ms() const {
            return m_period_ms;
        }
    };

} // namespace tempo
