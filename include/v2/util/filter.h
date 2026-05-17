/**
 * @file filter.h
 * @brief Stateless EMA scalar + snapshot overloads.
 */
#pragma once

#include <cstdint>
#include <cstdlib>

#include "v2/events.h"

namespace pocketpd {

    class Filter {
    public:
        /**
         * @brief One EMA step. `new = ((den-1)*prev + sample) / den`.
         * For `den = 4`: weights 0.75 prev / 0.25 sample.
         *
         * @param prev The previous value.
         * @param sample The new value.
         * @param den The denominator of the EMA.
         * @param snap The snap threshold.
         * @return The EMA value.
         */
        static uint32_t ema(
            uint32_t prev, uint32_t sample, uint32_t den, uint32_t snap = UINT32_MAX
        ) {
            uint32_t diff = sample > prev ? sample - prev : prev - sample;
            if (diff >= snap) {
                return sample;
            }
            return (prev * (den - 1) + sample) / den;
        }
    };

} // namespace pocketpd
