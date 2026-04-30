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
     * @tparam Conductor The concrete Conductor instantiation that owns this Stage. Forward
     * declaration is sufficient.
     */
    template <typename ...Stages>
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
