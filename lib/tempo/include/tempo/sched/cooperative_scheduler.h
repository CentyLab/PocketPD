// src/tempo/sched/CooperativeScheduler.h
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tempo/sched/task.h"

namespace tempo {

    /**
     * @brief Cooperative scheduler for the variant-event model. Drains the event queue
     * first (dispatching each event to every Task whose Stage filter matches),
     * then ticks every Task whose period has elapsed.
     *
     * @tparam StageId An enum type representing all possible stages.
     * @tparam TEvent An event type. Usually std::variant<...>.
     * @tparam MaxTasks The maximum number of tasks that can be added to the scheduler.
     * @tparam QueueCapacity The capacity of the event queue.
     */
    template <typename TStageId, typename TEvent, size_t MaxTasks>
    class CooperativeScheduler {
        using task_t = Task<TStageId, TEvent>;

        std::array<task_t*, MaxTasks> m_tasks{};
        size_t m_count = 0;

    public:
        bool add(task_t& t) {
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
         *
         * @param event The event to route.
         * @param stage The stage to route the event to.
         * @param now_ms The current time in milliseconds.
         */
        void dispatch_event(const TEvent& event, TStageId current_stage, uint32_t now_ms) {
            for (size_t i = 0; i < m_count; i++) {
                auto* t = m_tasks[i];
                if (t->allowed_stages().contains(current_stage)) {
                    t->on_event(event, now_ms);
                }
            }
        }

        /**
         * @brief Run periodic on_tick on every task whose stage filter matches and
         * whose period has elapsed.
         *
         * @param now_ms The current time in milliseconds.
         * @param current The current stage.
         */
        void tick(uint32_t now_ms, TStageId current_stage) {
            for (size_t i = 0; i < m_count; ++i) {
                auto* t = m_tasks[i];
                if (t->allowed_stages().contains(current_stage)) {
                    t->tick(now_ms);
                }
            }
        }

        /**
         * @brief Notify all tasks that the stage has changed.
         *
         * @param from The previous stage.
         * @param to The new stage.
         */
        void notify_stage_changed(TStageId from, TStageId to) {
            for (size_t i = 0; i < m_count; i++) {
                auto* t = m_tasks[i];
                if (t->allowed_stages().contains(from) || t->allowed_stages().contains(to)) {
                    t->on_stage_changed(from, to);
                }
            }
        }
        size_t task_count() const {
            return m_count;
        }

        task_t* task_at(size_t index) const {
            return index < m_count ? m_tasks[index] : nullptr;
        }
    };
} // namespace tempo
