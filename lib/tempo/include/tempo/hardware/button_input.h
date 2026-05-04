/**
 * @file button_input.h
 * @brief Abstract momentary-button input.
 *
 */
#pragma once

namespace tempo {

    class ButtonInput {
    public:
        virtual ~ButtonInput() = default;

        /// Drive debounce state. Call once per task tick.
        virtual void update() = 0;
        virtual bool is_held() const = 0;
    };

} // namespace tempo
