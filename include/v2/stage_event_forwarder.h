/**
 * @file stage_event_forwarder.h
 * @brief Helper task to forward events to the owning stage. The stage must implement
 * `on_event(const Event&, uint32_t now_ms)`. This allows the stage to react to events by passing
 * the current limitation, as Tempo does not support stage-level event handling at this moment.
 */
#pragma once

#include <cstdint>
#include <limits>

#include "v2/app.h"

namespace pocketpd {

    template <typename OwnerStage>
    class StageEventForwarder : public App::StageScopedTask {
    private:
        OwnerStage& m_owner;

    public:
        explicit StageEventForwarder(OwnerStage& owner)
            : App::StageScopedTask(
                  std::numeric_limits<uint32_t>::max(), App::StageMask::template of<OwnerStage>()
              ),
              m_owner(owner) {}

        const char* name() const override {
            return "StageEventForwarder";
        }

        void on_event(const Event& event, uint32_t now_ms) override {
            m_owner.on_event(event, now_ms);
        }
    };

} // namespace pocketpd
