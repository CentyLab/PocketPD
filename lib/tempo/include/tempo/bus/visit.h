#pragma once

namespace tempo {

    /**
     * @brief Aggregate of callable types whose `operator()`s overload one another.
     * See reference: https://en.cppreference.com/cpp/utility/variant/visit2.
     *
     * Example:
     * @code
     *   std::visit(tempo::overloaded{
     *       [&](const ButtonEvent& e) { handle_button(e); },
     *       [&](const EncoderEvent& e) { handle_encoder(e); },
     *       [](const auto&) {}, // ignore everything else
     *   }, event);
     * @endcode
     *
     * The catch-all `[](const auto&) {}` is required when the variant has alternatives the call
     * site does not care about — `std::visit` requires exhaustive coverage. For a
     * `tempo::Events<...>` variant in particular, `std::monostate` (the default-constructed
     * sentinel) needs an explicit handler or the catch-all.
     */
    template <typename... Fs>
    struct overloaded : Fs... {
        using Fs::operator()...;
    };

    template <typename... Fs>
    overloaded(Fs...) -> overloaded<Fs...>;

} // namespace tempo
