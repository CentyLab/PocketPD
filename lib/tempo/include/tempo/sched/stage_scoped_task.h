#pragma once

#include "tempo/sched/periodic_task.h"

namespace tempo {

    /**
     * @brief Stage-scoped Task. Runs only in stages listed in the mask supplied at
     * construction.
     *
     * @tparam Event The application's event type.
     * @tparam Stages The compile-time stage type list.
     */
    template <typename Event, typename... Stages>
    class StageScopedTask : public PeriodicTask<Event, Stages...> {
    public:
        using typename Task<Event, Stages...>::StageMaskType;

    private:
        StageMaskType m_scope;

    public:
        StageScopedTask(uint32_t period_ms, StageMaskType scope)
            : PeriodicTask<Event, Stages...>(period_ms), m_scope(scope) {}

        StageMaskType allowed_stages() const final {
            return m_scope;
        }
    };

} // namespace tempo
