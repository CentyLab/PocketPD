#pragma once

#include <cstdint>
#include <variant>

namespace tempo {

    struct StageChangedEvent {
        uint8_t from = 0;
        uint8_t to = 0;
    };

    template <typename... UserEvents>
    using Events = std::variant<std::monostate, StageChangedEvent, UserEvents...>;
} // namespace tempo
