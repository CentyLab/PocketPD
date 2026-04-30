#pragma once

#include <cstddef>
#include <type_traits>

namespace tempo {

    // —— Compile-time 0-based index of T in Ts...
    //
    // Resolved via recursive template specialization:
    //   - base case:      T at head of pack          -> 0
    //   - recursive case: some other U at head       -> 1 + type_index<T, tail...>
    //   - T not in pack:  primary template (undefined) selected -> compile error
    //
    // This is what makes register_stage<S> / request<S> reject types that aren't in
    // the stage list. type_index_v is the usual variable-template shortcut for
    // ::value.

    template <typename T, typename... Ts>
    struct type_index;

    template <typename T, typename... Rest>
    struct type_index<T, T, Rest...> : std::integral_constant<size_t, 0> {};

    template <typename T, typename U, typename... Rest>
    struct type_index<T, U, Rest...>
        : std::integral_constant<size_t, 1 + type_index<T, Rest...>::value> {};

    template <typename T, typename... Ts>
    constexpr inline size_t type_index_v = type_index<T, Ts...>::value;

} // namespace tempo
