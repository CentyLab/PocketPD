#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "tempo/bus/event_queue.h"
#include "tempo/bus/publisher.h"
#include "tempo/core/panic.h"
#include "tempo/diag/logger.h"
#include "tempo/hardware/stream.h"
#include "tempo/sched/background_task.h"
#include "tempo/sched/cooperative_scheduler.h"
#include "tempo/sched/periodic_task.h"
#include "tempo/sched/stage_scoped_task.h"
#include "tempo/sched/task.h"
#include "tempo/stage/conductor.h"

namespace tempo {

    constexpr size_t DEFAULT_MAX_TASKS = 16;
    constexpr size_t DEFAULT_EVENT_QUEUE_CAP = 32;

    /**
     * @brief Single-core application orchestrator.
     *
     * Application is a thin wrapper around the scheduler, conductor, and event queues. It
     * does not own any hardware, drivers, app data, or other product-specific runtime data.
     *
     * The two things it borrows by reference: Clock and StreamWriter.
     *
     * Anything else (Display, inputs, Storage, sensors) is wired directly from main.cpp into
     * whichever Task or Stage needs it.
     *
     * Stage identity is the Stage's type. The Stages... pack defines the slot order for the
     * Conductor and StageMask.
     *
     * @tparam Event An event type of std::variant<...>.
     * @tparam Stages The compile-time list of Stage types.
     */
    template <typename Event, typename... Stages>
    class Application : public UseLog<Application<Event, Stages...>> {
    public:
        // clang-format off
        static constexpr size_t MaxTasks           = DEFAULT_MAX_TASKS;
        static constexpr size_t EventQueueCapacity = DEFAULT_EVENT_QUEUE_CAP;

        using Conductor       = tempo::Conductor<Event, Stages...>;
        using Stage           = tempo::Stage<Event, Stages...>;
        using StageMask       = tempo::StageMask<Stages...>;

        using Task            = tempo::Task<Event, Stages...>;
        using PeriodicTask    = tempo::PeriodicTask<Event, Stages...>;
        using BackgroundTask  = tempo::BackgroundTask<Event, Stages...>;
        using StageScopedTask = tempo::StageScopedTask<Event, Stages...>;

        using Scheduler       = tempo::CooperativeScheduler<Event, MaxTasks, Stages...>;
        using Queue           = tempo::EventQueue<Event, EventQueueCapacity>;
        using Publisher       = tempo::QueuePublisher<Event, EventQueueCapacity>;

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

        const char* name_of_stage_at(size_t idx) const {
            return m_conductor.name_at(idx);
        }

    public:
        Application(const Clock& clock, StreamWriter& stream_writer)
            : m_clock(clock),
              m_stream_writer(stream_writer),
              m_task_publisher(m_task_queue),
              m_isr_publisher(m_isr_queue) {

            TEMPO_CHECK(instance == nullptr, "Only one Application may exist per build");
            instance = this;

            this->_uselog_wiring.attach_log(m_clock, m_stream_writer);
        }

        ~Application() = default;

        // ---- Registration (before start) ----
        template <typename T>
        bool add_task(T& task) {
            if constexpr (std::is_base_of_v<UseLog<T>, T>) {
                auto& use_log = static_cast<UseLog<T>&>(task);
                use_log._uselog_wiring.attach_log(m_clock, m_stream_writer);
            }

            return m_scheduler.add(task);
        }

        template <typename S>
        void register_stage(S& stage) {
            if constexpr (std::is_base_of_v<UseLog<S>, S>) {
                auto& use_log = static_cast<UseLog<S>&>(stage);
                use_log._uselog_wiring.attach_log(m_clock, m_stream_writer);
            }

            m_conductor.register_stage(stage);
        }

        /**
         * @brief Setup services and start the application.
         *
         * Unregistered stage slots fall back to a no-op NullStage, so calling start with no
         * stages registered is allowed but staging functionality is effectively disabled.
         *
         * @tparam InitialStage The stage type to enter on startup.
         */
        template <typename InitialStage>
        void start() {
            TEMPO_CHECK(!m_started, "Application::start called twice");

            m_scheduler.start();
            m_conductor.template start<InitialStage>();

            m_started = true;
            log.info("tempo: started, Stage=%s", m_conductor.current_name());
        }

        /**
         * @brief Main tick loop.
         */
        void tick() {
            if (!m_started) {
                return;
            }

            const uint32_t now = m_clock.now_ms();

            // 1. Apply any pending Stage transition from the previous tick.
            const size_t before = m_conductor.current_index();
            if (m_conductor.apply_pending_transition()) {
                const size_t after = m_conductor.current_index();
                m_scheduler.notify_stage_changed(before, after);
                log.info("Stage %s -> %s", name_of_stage_at(before), name_of_stage_at(after));
            }

            const size_t current = m_conductor.current_index();

            // 2. Drain ISR queue first (more sensitive), then task queue. Per event, the
            // scheduler dispatches to every matching task before the conductor delivers the
            // same event to the active stage's on_event override.
            Event e;
            while (m_isr_queue.pop(e)) {
                m_scheduler.dispatch_event(e, current, now);
                m_conductor.dispatch_event(e, now);
            }
            while (m_task_queue.pop(e)) {
                m_scheduler.dispatch_event(e, current, now);
                m_conductor.dispatch_event(e, now);
            }

            // 3. Run periodic on_tick on every task whose stage filter matches.
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

        size_t current_stage_index() const {
            return m_conductor.current_index();
        }
    };

} // namespace tempo
