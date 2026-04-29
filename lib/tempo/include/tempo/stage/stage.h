#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "tempo/core/time.h"

namespace tempo {

    template <typename TStageId>
    class Conductor;

    template <typename TStageId>
    class Stage {
    private:
        friend class Conductor<TStageId>;
        std::optional<std::reference_wrapper<const Clock>> m_clock;

        void set_clock(const Clock& clock) {
            m_clock = clock;
        }

    public:
        using ConductorType = tempo::Conductor<TStageId>;

        virtual ~Stage() = default;

        virtual const char* name() const {
            return "<unnamed_stage>";
        };

        virtual void on_enter(ConductorType& conductor) {};
        virtual void on_exit(ConductorType& conductor) {};
        virtual void on_tick(ConductorType& conductor, uint32_t now_ms) {};
    };
} // namespace tempo
