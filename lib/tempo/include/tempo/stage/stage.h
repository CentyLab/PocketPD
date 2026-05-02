#pragma once

#include <cstdint>

namespace tempo {

    template <typename... Stages>
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
     * @tparam Conductor The concrete Conductor instantiation that owns this Stage. Forward
     * declaration is sufficient.
     */
    template <typename... Stages>
    class Stage {
    public:
        using Conductor = tempo::Conductor<Stages...>;
        virtual ~Stage() = default;

        virtual const char* name() const {
            return "<unnamed_stage>";
        }

        virtual void on_enter(Conductor& conductor) {
            //
        }
        virtual void on_exit(Conductor& conductor) {
            //
        }
        virtual void on_tick(Conductor& conductor, uint32_t now_ms) {
            //
        }
    };

} // namespace tempo
