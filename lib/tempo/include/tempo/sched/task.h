#pragma once

#include <cstddef>
#include <cstdint>

#include "tempo/stage/stage_mask.h"

namespace tempo {

    /**
     * @brief Task interface.
     *
     * @tparam Event The application's event type, passed to `on_event`.
     * @tparam Stages The compile-time stage type list, used to derive the `StageMask`.
     */
    template <typename Event, typename... Stages>
    class Task {
    public:
        using StageMaskType = StageMask<Stages...>;

        virtual ~Task() = default;

        /**
         * @brief Required. The period of the Task in milliseconds.
         */
        virtual uint32_t period_ms() const = 0;

        /**
         * @brief Required. Called on every scheduler tick if the Task's period has elapsed
         * and the current Stage is in allowed_stages().
         */
        virtual void tick(uint32_t now_ms) = 0;

        // —— Optional methods

        virtual const char* name() const {
            return "<unnamed_task>";
        }

        virtual void on_start() {}

        /**
         * @brief Called once per event in the queue, in order, for every Task whose Stage
         * filter matches.
         */
        virtual void on_event(const Event& event, uint32_t now_ms) {}

        /**
         * @brief Called when the Stage changes.
         */
        virtual void on_stage_changed(size_t from_idx, size_t to_idx) {}

        virtual StageMaskType allowed_stages() const {
            return StageMaskType::none();
        }
    };

} // namespace tempo
