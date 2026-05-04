/**
 * @file encoder_input.h
 * @brief Abstract rotary encoder input.
 *
 */
#pragma once

#include <cstdint>

namespace tempo {

    /**
     * @brief Abstract rotary encoder.
     */
    class EncoderInput {
    public:
        virtual ~EncoderInput() = default;
        virtual int32_t position() const = 0;
    };

} // namespace tempo
