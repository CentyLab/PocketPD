#pragma once

#include <variant>

namespace tempo {

    template <typename... UserEvents>
    using Events = std::variant<std::monostate, UserEvents...>;

} // namespace tempo
