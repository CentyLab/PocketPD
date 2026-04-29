// src/tempo/sched/BackgroundTask.h
#pragma once

#include <tempo/stage/stage_mask.h>

#include "periodic_task.h"

namespace tempo {
    /**
     * @brief Background Task. Runs in every Stage.
     *
     * @tparam TStageId An enum type representing all possible stages.
     * @tparam TEvent An event type. Usually std::variant<...>.
     */
    template <typename TStageId, typename TEvent>
    class BackgroundTask : public PeriodicTask<TStageId, TEvent> {
    public:
        using PeriodicTask<TStageId,TEvent>::PeriodicTask;
        using stage_mask_t = StageMask<TStageId>;

        stage_mask_t allowed_stages() const final {
            return stage_mask_t::all();
        }
    };

} // namespace tempo
