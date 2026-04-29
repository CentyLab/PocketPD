#pragma once

#include <cstdint>

namespace tempo {

    enum class PanicReason : uint8_t {
        Unspecified = 0,
        AssertFailed = 1,
        StackOverflow = 2,
        DeadlockDetected = 3,
        HwFault = 4,
        UserRequested = 5,
        ConfigCorrupt = 6,
    };

} // namespace tempo

#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_BUILD)

#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"

namespace tempo {

    /**
     * @brief Non-returning error path. Halts; optional watchdog scratch for post-mortem debug.
     * TODO: Need platform-specific implementation
     *
     * @param reason The reason for the panic.
     * @param file The file name.
     * @param line The line number.
     * @param msg The message to print.
     */
    [[noreturn]]
    inline void panic(PanicReason reason, const char* file, int line, const char* msg) noexcept {
        (void) reason;
        (void) file;
        (void) line;
        (void) msg;
        if (watchdog_hw != nullptr) {
            watchdog_hw->scratch[0] = 0xDEADBEEFU;
        }

        watchdog_reboot(0, 0, 0);
        for (;;) {
            // in case reboot returns, we want to hang here to prevent further execution
        }
    }
} // namespace tempo

#else

#include <cstdio>
#include <cstdlib>

namespace tempo {

    [[noreturn]]
    inline void panic(PanicReason reason, const char* file, int line, const char* msg) noexcept {
        std::fprintf(
            stderr, "tempo panic [%u] %s:%d: %s\n", static_cast<unsigned>(reason), file, line, msg);
        std::abort();
    }
} // namespace tempo

#endif

#define TEMPO_PANIC(reason, msg) ::tempo::panic((reason), __FILE__, __LINE__, (msg))

#define TEMPO_CHECK(cond, msg)                                                                     \
    do {                                                                                           \
        if (!(cond))                                                                               \
            TEMPO_PANIC(::tempo::PanicReason::AssertFailed, (msg));                                \
    } while (0)
