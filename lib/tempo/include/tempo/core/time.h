#pragma once

#include <cstdint>

namespace tempo {

    /**
     * @brief Wraparound-safe comparison helpers for 32-bit timestamps.
     *
     */
    inline int32_t time_diff(uint32_t now, uint32_t deadline) {
        return static_cast<int32_t>(now - deadline);
    }

    inline bool time_reached(uint32_t now, uint32_t deadline) {
        return time_diff(now, deadline) >= 0;
    }

    /**
     * @brief Implementation-agnostic Clock interface
     * 
     */
    class Clock {
    public:
        virtual ~Clock() = default;
        virtual uint32_t now_ms() const = 0;
    };

} // namespace tempo
