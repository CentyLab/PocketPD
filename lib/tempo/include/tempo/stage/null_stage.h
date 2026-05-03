#pragma once

#include "tempo/stage/stage.h"

namespace tempo {

    template <typename Event, typename... Stages>
    class NullStage : public Stage<Event, Stages...> {
    public:
        const char* name() const override {
            return "<null>";
        }
    };

} // namespace tempo
