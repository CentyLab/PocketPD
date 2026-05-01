#pragma once
#include <cstddef>
#include <cstdio>

namespace ap33772 {

    /**
     * Format all source PDOs into `out` as null-terminated multi-line text.
     *
     * Hardware-agnostic: caller forwards bytes to Serial, Logger, file,
     * ring buffer, or any other sink.
     *
     * Templated on a duck-typed `Sink` so the same body serves the raw
     * `ap33772::AP33772` driver and any consumer-side abstraction (e.g.
     * `pocketpd::PdSinkController`). The Sink must expose:
     *
     *   int  pdo_count()                   const;
     *   int  pps_count()                   const;
     *   bool is_index_pps(int)             const;
     *   int  pdo_min_voltage_mv(int)       const;
     *   int  pdo_max_voltage_mv(int)       const;
     *   int  pdo_max_current_ma(int)       const;
     *
     * @return Bytes written (excluding null terminator). Output is truncated to cap-1 if the buffer
     * is too small.
     */
    template <typename Sink>
    inline size_t format_pdo(const Sink& s, char* out, size_t cap) {
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

        append("Source PDO Number = %d\n", s.pdo_count());
        append("PPS Profiles = %d\n", s.pps_count());

        for (int i = 0; i < s.pdo_count(); i++) {
            if (s.is_index_pps(i)) {
                append(
                    "PDO[%d] - PPS : %.1fV~%.1fV @ %.1fA\n", i + 1,
                    s.pdo_min_voltage_mv(i) / 1000.0, s.pdo_max_voltage_mv(i) / 1000.0,
                    s.pdo_max_current_ma(i) / 1000.0
                );
            } else {
                append(
                    "PDO[%d] - Fixed : %.1fV @ %.1fA\n", i + 1, s.pdo_max_voltage_mv(i) / 1000.0,
                    s.pdo_max_current_ma(i) / 1000.0
                );
            }
        }

        append("===============================================\n");
        return total;
    }

} // namespace ap33772
