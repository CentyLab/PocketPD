#pragma once

#include <cstdint>

namespace tempo {

    template <typename Event, typename... Stages>
    class Conductor;

    /**
     * @brief Stage interface.
     *
     * Each Stage subclass represents one mode the application can be in. The Conductor that owns
     * the Stage is passed to every lifecycle callback so the Stage can request transitions
     * (`conductor.request<Other>()`) or read its current_index().
     *
     * Optional protocol — payload-passing entry: a stage may define a non-virtual member
     * `prepare(Args...)` to receive entry context from `Conductor::request<Self>(Args...)`. The
     * conductor calls `prepare` immediately at request time, before the transition is applied;
     * the subsequent `on_enter` reads whatever state `prepare` stashed. Not virtual because
     * payload signatures vary per stage; see `Conductor::request` docs.
     *
     * @tparam Event The application's event type, usually `std::variant<...>` or
     *               `tempo::Events<...>`. Used by `on_event` to receive bus events when this
     *               stage is active.
     * @tparam Stages The compile-time list of Stage types managed by the Conductor.
     */
    template <typename Event, typename... Stages>
    class Stage {
    public:
        using Conductor = tempo::Conductor<Event, Stages...>;
        virtual ~Stage() = default;

        virtual const char* name() const {
            return "<unnamed_stage>";
        }

        virtual void on_enter(Conductor&) {}
        virtual void on_exit(Conductor&) {}
        virtual void on_tick(Conductor&, uint32_t) {}

        /**
         * @brief Called by `Conductor::dispatch_event` when this stage is active.
         *
         * Default body is empty so stages opt in by overriding. The active stage receives every
         * event drained from the application's queues that tick. Inactive stages do not see
         * events; if cross-stage event handling is needed, route it through a `Task`.
         */
        virtual void on_event(Conductor&, const Event&, uint32_t) {}
    };

} // namespace tempo
