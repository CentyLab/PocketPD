#pragma once
#include <cstdint>

#include "tempo/stage/stage_mask.h"

namespace tempo {

    /**
     * @brief Task interface.
     *
     * @tparam TStageId An enum type representing all possible stages.
     * @tparam TEvent An event type. Usually std::variant<...>.
     */
    template <typename TStageId, typename TEvent>
    class Task {
    public:
        using stage_mask_t = StageMask<TStageId>;

        virtual ~Task() = default;

        /**
         * @brief Required. The period of the Task in milliseconds.
         *
         * @return uint32_t
         */
        virtual uint32_t period_ms() const = 0;

        /**
         * @brief Required. This function is called on every scheduler tick if the Task's period has
         * elapsed and the current Stage is in allowed_stages().
         *
         * @param now_ms
         */
        virtual void tick(uint32_t now_ms) = 0;

        // —— Optional methods

        virtual const char* name() const {
            return "<unnamed_task>";
        }

        /**
         * @brief Lifecycle. Called when the Task is started.
         *
         */
        virtual void on_start() {}

        /**
         * @brief Event handling. Called once per event in the queue, in order, for every
         * Task whose Stage filter matches. The Task inspects the variant and
         * chooses what to do with it.
         *
         * Default implementation ignores all events — a Task that doesn't care
         * about events simply doesn't override this.
         *
         * @param e The event.
         * @param now_ms The current time in milliseconds.
         */
        virtual void on_event(const TEvent& event, uint32_t now_ms) {}

        /**
         * @brief Lifecycle. Called when the Task's Stage changes.
         *
         */
        virtual void on_stage_changed(TStageId from, TStageId to) {}

        /**
         * @brief The stages in which the Task is allowed to run.
         *
         * @return stage_mask_t
         */
        virtual stage_mask_t allowed_stages() const {
            return stage_mask_t::none();
        }
    };
} // namespace tempo
