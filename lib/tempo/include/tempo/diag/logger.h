#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

// Arduino's `F()` macro collides with fmt's template parameter named `F`. Save and
// restore around the fmt include so downstream Arduino code keeps the macro.
#pragma push_macro("F")
#ifdef F
#undef F
#endif
#include <fmt/format.h>
#pragma pop_macro("F")

#include "tempo/core/time.h"
#include "tempo/diag/stream_iterator.h"
#include "tempo/hardware/stream.h"

// 0=NONE 1=ERROR 2=WARN 3=INFO 4=DEBUG 5=VERBOSE
// Override with -DLOG_LEVEL=N in platformio.ini build_flags.
#ifndef LOG_LEVEL
#if defined(NDEBUG) || defined(RELEASE_BUILD)
#define LOG_LEVEL 2
#else
#define LOG_LEVEL 5
#endif
#endif

namespace tempo {

    enum class LogLevel : uint8_t {
        NONE = 0,
        ERROR = 1,
        WARN = 2,
        INFO = 3,
        DEBUG = 4,
        VERBOSE = 5,
    };

    inline constexpr uint8_t COMPILE_LEVEL = LOG_LEVEL;

    constexpr bool compiledIn(LogLevel level) {
        return static_cast<uint8_t>(level) <= COMPILE_LEVEL;
    }

    constexpr char level_tag(const LogLevel l) {
        switch (l) {
        case LogLevel::ERROR:
            return 'E';
        case LogLevel::WARN:
            return 'W';
        case LogLevel::INFO:
            return 'I';
        case LogLevel::DEBUG:
            return 'D';
        case LogLevel::VERBOSE:
            return 'V';
        case LogLevel::NONE:
            return '?';
        }
        return '?';
    }

    constexpr const char* level_color(const LogLevel l) {
        switch (l) {
        case LogLevel::ERROR:
            return "\x1b[31m"; // red
        case LogLevel::WARN:
            return "\x1b[33m"; // yellow
        case LogLevel::INFO:
            return "\x1b[32m"; // green
        case LogLevel::DEBUG:
            return "\x1b[36m"; // cyan
        case LogLevel::VERBOSE:
            return "\x1b[90m"; // gray
        case LogLevel::NONE:
            return "";
        }
        return "";
    }

    constexpr const char* COLOR_RESET = "\x1b[0m";
    constexpr const char* COLOR_RESET_NEWLINE = "\x1b[0m\n";

    class Logger {
    private:
        const char* m_tag = nullptr;

        std::optional<std::reference_wrapper<const Clock>> m_clock;
        std::optional<std::reference_wrapper<StreamWriter>> m_stream_writer;

        template <LogLevel L, typename... Args>
        void log(fmt::format_string<Args...> fs, Args&&... args) const {
            if constexpr (!compiledIn(L)) {
                return;
            }

            // Guard. Logging should be valid after this check.
            if (disabled()) {
                return;
            }

            BufferedStreamSink sink(m_stream_writer->get());
            write_header(sink, L);
            fmt::format_to(sink.out(), fs, std::forward<Args>(args)...);
            fmt::format_to(sink.out(), "{}", COLOR_RESET_NEWLINE);
        }

        void write_header(BufferedStreamSink& sink, LogLevel level) const {
            // clang-format off
            const uint32_t ms_total  = m_clock->get().now_ms();
            const uint32_t s_total   = ms_total / 1000;
            const uint32_t ms        = ms_total - s_total * 1000;
            const uint32_t min_total = s_total / 60;
            const uint32_t sec       = s_total - min_total * 60;
            const uint32_t hr        = min_total / 60;
            const uint32_t min       = min_total - hr * 60;
            // clang-format on
            fmt::format_to(
                sink.out(),
                "{}[{:02}:{:02}:{:02}.{:03}][{}][{}] ",
                level_color(level),
                hr,
                min,
                sec,
                ms,
                level_tag(level),
                m_tag
            );
        }

        void hexdump_impl(const char* label, const uint8_t* data, size_t len) const {
            StreamWriter& sw = m_stream_writer->get();
            {
                BufferedStreamSink sink(sw);
                write_header(sink, LogLevel::DEBUG);
                if (label) {
                    fmt::format_to(sink.out(), "{} ({} bytes):", label, len);
                }
                fmt::format_to(sink.out(), "{}", COLOR_RESET_NEWLINE);
            }

            constexpr size_t ROW_WIDTH = 16;
            for (size_t i = 0; i < len; i += ROW_WIDTH) {
                BufferedStreamSink sink(sw);
                auto it = sink.out();
                fmt::format_to(it, "  {:04x}: ", i);
                for (size_t j = 0; j < ROW_WIDTH; ++j) {
                    if (i + j < len) {
                        fmt::format_to(it, "{:02x} ", data[i + j]);
                    } else {
                        fmt::format_to(it, "   ");
                    }
                }
                fmt::format_to(it, " ");
                for (size_t j = 0; j < ROW_WIDTH && i + j < len; ++j) {
                    const uint8_t c = data[i + j];
                    const char printable = static_cast<char>((c >= 0x20 && c < 0x7f) ? c : '.');
                    fmt::format_to(it, "{}", printable);
                }
                fmt::format_to(it, "\n");
            }
        }

    public:
        Logger() = default;
        explicit Logger(const char* tag, Clock& clock, StreamWriter& stream_writer)
            : m_tag(tag), m_clock(clock), m_stream_writer(stream_writer) {}

        void set_tag(const char* tag) {
            m_tag = tag;
        }

        void set_clock(const Clock& clock) {
            m_clock = clock;
        }

        void set_stream_writer(StreamWriter& stream_writer) {
            m_stream_writer = stream_writer;
        }

        bool enabled() const {
            return m_clock.has_value() && m_stream_writer.has_value();
        }

        bool disabled() const {
            return !m_clock.has_value() || !m_stream_writer.has_value();
        }

        template <typename... Args>
        void error(fmt::format_string<Args...> fs, Args&&... args) const {
            log<LogLevel::ERROR>(fs, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void warn(fmt::format_string<Args...> fs, Args&&... args) const {
            log<LogLevel::WARN>(fs, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void info(fmt::format_string<Args...> fs, Args&&... args) const {
            log<LogLevel::INFO>(fs, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void debug(fmt::format_string<Args...> fs, Args&&... args) const {
            log<LogLevel::DEBUG>(fs, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void verbose(fmt::format_string<Args...> fs, Args&&... args) const {
            log<LogLevel::VERBOSE>(fs, std::forward<Args>(args)...);
        }

        void hexdump(const char* label, const void* data, size_t len) const {
            if constexpr (!compiledIn(LogLevel::DEBUG)) {
                return;
            }

            if (disabled()) {
                return;
            }

            hexdump_impl(label, static_cast<const uint8_t*>(data), len);
        }

        template <typename... Args>
        void check(bool cond, fmt::format_string<Args...> fs, Args&&... args) const {
            if (!cond) {
                error(fs, std::forward<Args>(args)...);
            }
        }
    };

    template <typename T, typename = void>
    struct has_log_tag : std::false_type {};

    template <typename T>
    struct has_log_tag<T, std::void_t<decltype(T::LOG_TAG)>> : std::true_type {};

    template <typename Event, typename... Stages>
    class Application;

    template <typename Derived>
    class UseLog;

    namespace detail {

        /**
         * @brief Storage layer for UseLog. Owns the LogSlot and exposes `log` to the outer
         * mixin. Split out of UseLog so that Derived — friend of UseLog for CRTP enforcement —
         * does not also gain access to `m_log_slot`.
         *
         * @tparam Derived
         */
        template <typename Derived>
        class UseLogStorage {
            static constexpr const char* resolve_tag() {
                if constexpr (has_log_tag<Derived>::value) {
                    return Derived::LOG_TAG;
                } else {
                    return "<Unnamed>";
                }
            }

            class LogSlot {
                Logger m_log;

                void attach(const Clock& clock, StreamWriter& stream_writer) {
                    m_log.set_clock(clock);
                    m_log.set_stream_writer(stream_writer);
                    m_log.set_tag(UseLogStorage<Derived>::resolve_tag());
                }

                template <typename Event, typename... Stages>
                friend class tempo::Application;
                friend class UseLogStorage<Derived>;
            };

            LogSlot m_log_slot;

            template <typename Event, typename... Stages>
            friend class tempo::Application;
            friend class tempo::UseLog<Derived>;

        protected:
            UseLogStorage() = default;
            const Logger& log = m_log_slot.m_log;
        };

    } // namespace detail

    /**
     * @brief CRTP mixin that injects a Logger into Tasks and Stages registered with an
     * Application.
     *
     * Derived class get access to logging functions via `log` member reference.
     *
     * @tparam Derived The host class (CRTP).
     */
    template <typename Derived>
    class UseLog : public detail::UseLogStorage<Derived> {
        UseLog() = default;

        template <typename Event, typename... Stages>
        friend class Application;
        friend Derived;
    };

} // namespace tempo
