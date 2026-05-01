#pragma once
#include <cstddef>
#include <cstdio>

#include "AP33772.h"

namespace ap33772 {

    /**
     * Format all source PDOs into `out` as null-terminated multi-line text.
     *
     * Hardware-agnostic: caller forwards bytes to Serial, Logger, file,
     * ring buffer, or any other sink.
     *
     * @return Bytes written (excluding null terminator). Output is truncated to cap-1 if the buffer
     * is too small.
     */
    inline size_t format_pdo(const AP33772& ap, char* out, size_t cap) {
        if (cap == 0) {
            return 0;
        }

        size_t total = 0;
        auto append = [&](const char* fmt, auto... args) {
            if (total + 1 >= cap) {
                return;
            }

            const int n = snprintf(&out[total], cap - total, fmt, args...);
            if (n > 0) {
                total += static_cast<size_t>(n);
                if (total >= cap) {
                    total = cap - 1;
                }
            }
        };

        append("Source PDO Number = %d\n", ap.get_count_pdo());
        append("PPS Profiles = %d\n", ap.get_count_pps());

        for (int i = 0; i < ap.get_count_pdo(); i++) {
            if (ap.is_index_pps(i)) {
                append(
                    "PDO[%d] - PPS : %.1fV~%.1fV @ %.1fA\n", i + 1,
                    ap.get_pdo_min_voltage(i) / 1000.0, ap.get_pdo_max_voltage(i) / 1000.0,
                    ap.get_pdo_max_current(i) / 1000.0
                );
            } else {
                append(
                    "PDO[%d] - Fixed : %.1fV @ %.1fA\n", i + 1, ap.get_pdo_max_voltage(i) / 1000.0,
                    ap.get_pdo_max_current(i) / 1000.0
                );
            }
        }

        append("===============================================\n");
        return total;
    }

} // namespace ap33772
