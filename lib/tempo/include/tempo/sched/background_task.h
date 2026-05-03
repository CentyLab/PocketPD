#pragma once

#include "tempo/sched/periodic_task.h"

namespace tempo {

    /**
     * @brief Background Task. Runs in every Stage.
     *
     * @tparam Event The application's event type.
     * @tparam Stages The compile-time stage type list.
     */
    template <typename Event, typename... Stages>
    class BackgroundTask : public PeriodicTask<Event, Stages...> {
    public:
        using PeriodicTask<Event, Stages...>::PeriodicTask;
        using StageMaskType = StageMask<Stages...>;

        StageMaskType allowed_stages() const final {
            return StageMaskType::all();
        }
    };

} // namespace tempo
