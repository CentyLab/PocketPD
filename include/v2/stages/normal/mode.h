/**
 * @file mode.h
 * @brief NormalStage sub-mode variant.
 */
#pragma once

#include <variant>

#include "v2/stages/normal/fixed_mode.h"
#include "v2/stages/normal/passthrough_mode.h"
#include "v2/stages/normal/pps_mode.h"

namespace pocketpd {

    using Mode = std::variant<PassthroughMode, FixedMode, PPSMode>;

} // namespace pocketpd
