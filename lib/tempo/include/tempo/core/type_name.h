#pragma once

#include <array>
#include <string_view>

namespace tempo::detail {

    template <typename T>
    constexpr std::string_view wrapped_type_name() {
#if defined(__clang__) || defined(__GNUC__)
        return __PRETTY_FUNCTION__;
#else
        return "";
#endif
    }

    template <typename T>
    constexpr std::string_view type_name_view() {
        constexpr auto raw = wrapped_type_name<T>();
#if defined(__clang__)
        // "std::string_view detail::wrapped_type_name() [T = MyType]"
        constexpr auto start = raw.find("T = ") + 4;
        constexpr auto end = raw.rfind(']');
        constexpr auto len = end - start;
        return raw.substr(start, len);
#elif defined(__GNUC__)
        // "constexpr std::string_view detail::wrapped_type_name() [with T = MyType; ...]"
        constexpr auto start = raw.find("T = ") + 4;
        constexpr auto end = raw.find(';', start);
        constexpr auto len = end - start;
        return raw.substr(start, len);
#else
        return raw;
#endif
    }

    template <typename T>
    constexpr auto make_type_name_array() {
        constexpr auto sv = type_name_view<T>();
        std::array<char, sv.size() + 1> arr{};
        for (size_t i = 0; i < sv.size(); i++) {
            arr[i] = sv[i];
        }

        arr[sv.size()] = '\0';
        return arr;
    }

    template <typename T>
    struct type_name_holder {
        static constexpr auto value = make_type_name_array<T>();
    };

} // namespace tempo::detail

namespace tempo {
    template <typename T>
    constexpr const char* type_name() {
        return detail::type_name_holder<T>::value.data();
    }
} // namespace tempo
