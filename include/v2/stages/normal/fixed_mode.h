/**
 * @file fixed_mode.h
 * @brief Fixed-PDO sub-mode for NormalStage.
 */
#pragma once

#include <cstdint>

namespace pocketpd {

    struct FixedMode {
        int32_t pdo_max_mv = 0;
        int32_t pdo_max_ma = 0;
    };

} // namespace pocketpd
