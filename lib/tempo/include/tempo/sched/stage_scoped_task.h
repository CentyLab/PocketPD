#pragma once

#include "tempo/sched/periodic_task.h"

namespace tempo {

    /**
     * @brief Stage-scoped Task. Runs only in stages listed in the mask supplied at
     * construction.
     *
     * @tparam Conductor The concrete Conductor instantiation.
     * @tparam TEvent An event type. Usually std::variant<...>.
     */
    template <typename Conductor, typename TEvent>
    class StageScopedTask : public PeriodicTask<Conductor, TEvent> {
    public:
        using StageMaskType = typename Conductor::StageMaskType;

    private:
        StageMaskType m_scope;

    public:
        StageScopedTask(uint32_t period_ms, StageMaskType scope)
            : PeriodicTask<Conductor, TEvent>(period_ms), m_scope(scope) {}

        StageMaskType allowed_stages() const final {
            return m_scope;
        }
    };

} // namespace tempo
