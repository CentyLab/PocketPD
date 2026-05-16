/**
 * @file filter.h
 * @brief Stateless EMA scalar + snapshot overloads.
 */
#pragma once

#include <cstdint>

#include "v2/events.h"

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

        /** @brief Per-field EMA across a `LoadReading`. `timestamp_ms` tracks the latest. */
        static LoadReading ema(const LoadReading& prev, const LoadReading& sample, uint32_t den) {
            return LoadReading{
                .timestamp_ms = sample.timestamp_ms,
                .vbus_mv = ema(prev.vbus_mv, sample.vbus_mv, den),
                .current_ma = ema(prev.current_ma, sample.current_ma, den),
            };
        }
    };

} // namespace pocketpd
