#pragma once

#include "tempo/sched/periodic_task.h"
#include "tempo/stage/stage_mask.h"

namespace tempo {

    /**
     * @brief Stage-scoped Task. Runs only in stages listed in the mask supplied at construction.
     *
     * @tparam TStageId
     * @tparam TEvent
     */
    template <typename TStageId, typename TEvent>
    class StageScopedTask : public PeriodicTask<TStageId, TEvent> {
        using stage_mask_t = StageMask<TStageId>;
        stage_mask_t m_scope;

    public:
        /**
         * @brief Construct a new Stage-scoped Task object.
         *
         * @param period_ms The period of the Task in milliseconds.
         * @param scope The stages in which the Task is allowed to run.
         */
        StageScopedTask(uint32_t period_ms, stage_mask_t scope)
            : PeriodicTask<TStageId, TEvent>(period_ms), m_scope(scope) {}

        /**
         * @brief The stages in which the Task is allowed to run.
         *
         * @return StageMask<TStageId>
         */
        stage_mask_t allowed_stages() const final {
            return m_scope;
        }
    };

} // namespace tempo
