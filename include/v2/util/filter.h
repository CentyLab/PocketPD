/**
 * @file filter.h
 * @brief Scalar filtering helpers.
 */
#pragma once

#include <cstdint>

namespace pocketpd {

    class Filter {
    public:
        /**
         * @brief One EMA step. `new = ((den-1)*prev + sample) / den`.
         * For `den = 4`: weights 0.75 prev / 0.25 sample.
         */
        static uint32_t ema(uint32_t prev, uint32_t sample, uint32_t den) {
            return (prev * (den - 1) + sample) / den;
        }
    };

} // namespace pocketpd
