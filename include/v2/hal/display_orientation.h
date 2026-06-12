/**
 * @file display_orientation.h
 * @brief Abstract control over the panel mounting orientation.
 *
 */
#pragma once

namespace pocketpd {

    class DisplayOrientation {
    public:
        virtual ~DisplayOrientation() = default;

        /**
         * @brief Rotate all subsequent rendering by 180 degrees (true) or restore the native
         * orientation (false). Takes effect on the next flush.
         */
        virtual void set_flipped(bool flipped) = 0;
    };

} // namespace pocketpd
