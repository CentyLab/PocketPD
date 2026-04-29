#pragma once

#include <limits>

#include "tempo/core/time.h"
#include "tempo/sched/task.h"
#include "tempo/stage/stage_mask.h"

namespace tempo {

    template <typename StageId, typename Event>
    class PeriodicTask : public Task<StageId, Event> {
        using stage_mask_t = StageMask<StageId>;

        uint32_t m_period = std::numeric_limits<uint32_t>::max();
        uint32_t m_next_deadline = 0;
        bool m_armed = false;

    protected:
        virtual void on_tick(uint32_t now_ms) {
            // do nothing unless overriden
        }

    public:
        explicit PeriodicTask(uint32_t period_ms) : m_period(period_ms) {}

        uint32_t period_ms() const final {
            return m_period;
        }

        /**
         * @brief Run immediate if not armed, otherwise check if the period has elapsed and run if
         * it has.
         *
         * @param now_ms
         */
        void tick(uint32_t now_ms) final {
            if (!m_armed) {
                m_next_deadline = now_ms + m_period;
                m_armed = true;
                on_tick(now_ms);
                return;
            }

            if (!time_reached(now_ms, m_next_deadline)) {
                return;
            }

            m_next_deadline += m_period;

            // Catch-up clamp. Prevents burst-firing after long stall.
            if (time_diff(now_ms, m_next_deadline) >= static_cast<int32_t>(m_period)) {
                m_next_deadline = now_ms + m_period;
            }

            on_tick(now_ms);
        }

        stage_mask_t allowed_stages() const {
            return stage_mask_t::all();
        }
    };

} // namespace tempo
