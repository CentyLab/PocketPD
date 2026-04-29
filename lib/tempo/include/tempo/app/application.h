#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "tempo/bus/event_queue.h"
#include "tempo/bus/publisher.h"
#include "tempo/core/panic.h"
#include "tempo/diag/logger.h"
#include "tempo/hardware/stream.h"
#include "tempo/sched/cooperative_scheduler.h"
#include "tempo/stage/conductor.h"

namespace tempo {

    constexpr size_t DEFAULT_MAX_TASKS = 16;
    constexpr size_t DEFAULT_EVENT_QUEUE_CAP = 32;

    /**
     * @brief Single-core application orchestrator.
     *
     * Application is a thin wrapper around the scheduler, conductor, and event queues. It does not
     * own any hardware, drivers, app data, or other product-specific runtime data.
     *
     * The two things it borrows by reference: Clock and StreamWriter.
     *
     * Anything else (Display, inputs, Storage, sensors) is wired directly from
     * main.cpp into whichever Task or Stage needs it.
     *
     * @tparam StageId An enum type representing all possible stages.
     * @tparam Event An event type of std::variant<...>.
     * @tparam MaxTasks The maximum number of tasks that can be added to the scheduler.
     * @tparam EventQueueCapacity The capacity of the event queue.
     */
    template <
        typename StageId,
        typename Event,
        size_t MaxTasks = DEFAULT_MAX_TASKS,
        size_t EventQueueCapacity = DEFAULT_EVENT_QUEUE_CAP>
    class Application : public UseLog<Application<StageId, Event, MaxTasks, EventQueueCapacity>> {
    public:
        // clang-format off
        using Scheduler  = tempo::CooperativeScheduler<StageId, Event, MaxTasks>;
        using Queue      = tempo::EventQueue<Event, EventQueueCapacity>;
        using Publisher  = tempo::QueuePublisher<Event, EventQueueCapacity>;
        using Conductor  = tempo::Conductor<StageId>;
        using Task       = tempo::Task<StageId, Event>;
        using Stage      = tempo::Stage<StageId>;
        using UseLog<Application>::log;
        // clang-format on

    private:
        static inline Application* instance = nullptr;

        constexpr static const char* LOG_TAG = "Application";

        // —— External references

        const Clock& m_clock;
        StreamWriter& m_stream_writer;

        // —— Owned subsystems

        Queue m_task_queue;
        Queue m_isr_queue;
        Publisher m_task_publisher;
        Publisher m_isr_publisher;

        Scheduler m_scheduler;
        Conductor m_conductor;

        bool m_started = false;

        static const char* name_of_current_stage() {
            return instance ? instance->m_conductor.current_name() : "?";
        }

        const char* name_of_stage(StageId id) const {
            const Stage* s = m_conductor.stage_for(id);
            return s ? s->name() : "?";
        }

    public:
        Application(const Clock& clock, StreamWriter& stream_writer)
            : m_clock(clock),
              m_stream_writer(stream_writer),
              m_conductor(clock),
              m_task_publisher(m_task_queue),
              m_isr_publisher(m_isr_queue) {

            TEMPO_CHECK(instance == nullptr, "Only one Application may exist per build");
            instance = this;

            this->attach_log(m_clock, m_stream_writer);
        }

        ~Application() = default;

        // ---- Registration (before start) ----
        template <typename T>
        bool add_task(T& task) {
            if constexpr (std::is_base_of_v<UseLog<T>, T>) {
                auto& use_log = static_cast<UseLog<T>&>(task);
                use_log.attach_log(m_clock, m_stream_writer);
            }

            return m_scheduler.add(task);
        }

        template <typename T>
        void register_stage(StageId id, T& stage) {
            if constexpr (std::is_base_of_v<UseLog<T>, T>) {
                auto& use_log = static_cast<UseLog<T>&>(stage);
                use_log.attach_log(m_clock, m_stream_writer);
            }

            m_conductor.register_stage(id, stage);
        }

        /**
         * @brief Setup services and start the application.
         *
         * Unregistered stage slots fall back to a no-op NullStage, so calling start with no stages
         * registered is allowed but staging functionality is effective disabled
         *
         * @param initial_stage The stage to enter on startup.
         */
        void start(StageId initial_stage) {
            TEMPO_CHECK(!m_started, "Application::start called twice");

            m_scheduler.start();
            m_conductor.start(initial_stage);

            m_started = true;
            log.info("tempo: started, Stage=%s", m_conductor.current_name());
        }

        /**
         * @brief Main tick loop
         *
         */
        void tick() {
            if (!m_started) {
                return;
            }

            const uint32_t now = m_clock.now_ms();

            // 1. Apply any pending Stage transition from the previous tick.
            const StageId before = m_conductor.current_id();
            if (m_conductor.apply_pending_transition()) {
                const StageId after = m_conductor.current_id();
                m_scheduler.notify_stage_changed(before, after);
                log.info("Stage %s -> %s", name_of_stage(before), name_of_stage(after));
            }

            const StageId current = m_conductor.current_id();

            // 2. Drainer: pop ISR queue first as it's more sensitive, then the task queue.
            //    Each event is dispatched to every task whose stage filter matches.
            Event e;
            while (m_isr_queue.pop(e)) {
                m_scheduler.dispatch_event(e, current, now);
            }

            while (m_task_queue.pop(e)) {
                m_scheduler.dispatch_event(e, current, now);
            }

            // 3. Run periodic on_tick on every task whose stage matches.
            m_scheduler.tick(now, current);

            // 4. Give the current stage a chance to run its own on_tick.
            m_conductor.tick(now);
        }

        // —— Accessors
        Queue& task_queue() {
            return m_task_queue;
        }

        Queue& isr_queue() {
            return m_isr_queue;
        }

        Publisher& task_publisher() {
            return m_task_publisher;
        }

        Publisher& isr_publisher() {
            return m_isr_publisher;
        }

        Scheduler& scheduler() {
            return m_scheduler;
        }

        Conductor& conductor() {
            return m_conductor;
        }

        StageId current_stage() const {
            return m_conductor.current_id();
        }
    };

} // namespace tempo
