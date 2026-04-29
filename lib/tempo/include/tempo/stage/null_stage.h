#pragma once

#include "tempo/stage/stage.h"

namespace tempo {

    template <typename TStageId>
    class NullStage : public Stage<TStageId> {
    public:
        const char* name() const override {
            return "<null>";
        }
    };

} // namespace tempo
