#pragma once

#include "tempo/sched/periodic_task.h"

namespace tempo {

    /**
     * @brief Background Task. Runs in every Stage.
     *
     * @tparam Conductor The concrete Conductor instantiation.
     * @tparam TEvent An event type. Usually std::variant<...>.
     */
    template <typename Conductor, typename Event>
    class BackgroundTask : public PeriodicTask<Conductor, Event> {
    public:
        using PeriodicTask<Conductor, Event>::PeriodicTask;
        using StageMaskType = typename Conductor::StageMaskType;

        StageMaskType allowed_stages() const final {
            return StageMaskType::all();
        }
    };

} // namespace tempo
