#pragma once

#include "tempo/stage/stage.h"

namespace tempo {

    template <typename... Stages>
    class NullStage : public Stage<Stages...> {
    public:
        const char* name() const override {
            return "<null>";
        }
    };

} // namespace tempo
