#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tempo/sched/task.h"

namespace tempo {

    /**
     * @brief Cooperative scheduler for the variant-event model. Drains the event queue first
     * (dispatching each event to every Task whose Stage filter matches), then ticks every
     * Task whose period has elapsed.
     *
     * @tparam Event The application's event type. Usually `std::variant<...>`.
     * @tparam MaxTasks The maximum number of tasks that can be added.
     * @tparam Stages The compile-time stage type list.
     */
    template <typename Event, size_t MaxTasks, typename... Stages>
    class CooperativeScheduler {
        using TaskType = Task<Event, Stages...>;

        std::array<TaskType*, MaxTasks> m_tasks{};
        size_t m_count = 0;

    public:
        bool add(TaskType& t) {
            if (m_count == MaxTasks) {
                return false;
            }
            m_tasks[m_count++] = &t;
            return true;
        }

        void start() {
            for (size_t i = 0; i < m_count; ++i) {
                m_tasks[i]->on_start();
            }
        }

        /**
         * @brief Route a single event to every task whose stage filter matches.
         */
        void dispatch_event(const Event& event, size_t current_idx, uint32_t now_ms) {
            for (size_t i = 0; i < m_count; ++i) {
                auto* t = m_tasks[i];
                if (t->allowed_stages().contains_index(current_idx)) {
                    t->on_event(event, now_ms);
                }
            }
        }

        /**
         * @brief Run periodic on_tick on every task whose stage filter matches and whose
         * period has elapsed.
         */
        void tick(uint32_t now_ms, size_t current_idx) {
            for (size_t i = 0; i < m_count; ++i) {
                auto* t = m_tasks[i];
                if (t->allowed_stages().contains_index(current_idx)) {
                    t->tick(now_ms);
                }
            }
        }

        /**
         * @brief Notify all tasks that the stage has changed.
         */
        void notify_stage_changed(size_t from_idx, size_t to_idx) {
            for (size_t i = 0; i < m_count; ++i) {
                auto* t = m_tasks[i];
                const auto mask = t->allowed_stages();
                if (mask.contains_index(from_idx) || mask.contains_index(to_idx)) {
                    t->on_stage_changed(from_idx, to_idx);
                }
            }
        }

        size_t task_count() const {
            return m_count;
        }

        TaskType* task_at(size_t index) const {
            return index < m_count ? m_tasks[index] : nullptr;
        }
    };

} // namespace tempo
