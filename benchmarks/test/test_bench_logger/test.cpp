/**
 * Native host benchmark: snprintf vs fmt::format_to_n.
 *
 * Purpose: relative-cost comparison for the format paths the Logger uses. Host CPU is
 * not RP2040; absolute numbers do not transfer. Ratios usually do (within an order of
 * magnitude); the on-device variant lives in `src/bench_logger.cpp` for true numbers.
 *
 * Run via: pio test -e native -f test_bench_logger
 */
#pragma GCC optimize("O3")

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#pragma push_macro("F")
#ifdef F
#undef F
#endif
#include <fmt/format.h>
#pragma pop_macro("F")

namespace {

    constexpr int N_ITERS = 100'000;
    constexpr size_t BUF_SIZE = 256;

    using clock_t_ = std::chrono::steady_clock;

    template <typename Fn>
    uint64_t time_ns(Fn&& fn) {
        auto t0 = clock_t_::now();
        fn();
        auto t1 = clock_t_::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }

    void anti_optimize(const char* p) {
        asm volatile("" : : "r"(p) : "memory");
    }

    void report(const char* label, uint64_t snp_ns, uint64_t fmt_ns) {
        const double snp_per = static_cast<double>(snp_ns) / N_ITERS;
        const double fmt_per = static_cast<double>(fmt_ns) / N_ITERS;
        const double ratio = fmt_per / snp_per;
        std::printf(
            "  %-32s snprintf=%7.1f ns/op  fmt=%7.1f ns/op  ratio=%4.2fx\n",
            label,
            snp_per,
            fmt_per,
            ratio
        );
    }

} // namespace

TEST(BenchLogger, SingleUint32) {
    char buf[BUF_SIZE];
    const uint32_t snp = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            std::snprintf(buf, BUF_SIZE, "v=%u", static_cast<unsigned>(i));
            anti_optimize(buf);
        }
    });
    const uint32_t fmtt = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            auto r = fmt::format_to_n(buf, BUF_SIZE, "v={}", i);
            *r.out = '\0';
            anti_optimize(buf);
        }
    });
    report("single uint32", snp, fmtt);
}

TEST(BenchLogger, MultiArgsTwoIntsString) {
    char buf[BUF_SIZE];
    const uint32_t snp = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            std::snprintf(
                buf, BUF_SIZE, "PD ready: %u PDO (%u PPS) %s", 7u, 3u, "Anker"
            );
            anti_optimize(buf);
        }
    });
    const uint32_t fmtt = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            auto r = fmt::format_to_n(buf, BUF_SIZE, "PD ready: {} PDO ({} PPS) {}", 7, 3, "Anker");
            *r.out = '\0';
            anti_optimize(buf);
        }
    });
    report("multi-arg (2 int + str)", snp, fmtt);
}

TEST(BenchLogger, TimestampHeader) {
    char buf[BUF_SIZE];
    const uint32_t snp = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            std::snprintf(
                buf,
                BUF_SIZE,
                "%s[%02lu:%02lu:%02lu.%03lu][%c][%s] ",
                "\x1b[32m",
                1ul,
                2ul,
                3ul,
                456ul,
                'I',
                "TestTag"
            );
            anti_optimize(buf);
        }
    });
    const uint32_t fmtt = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            auto r = fmt::format_to_n(
                buf,
                BUF_SIZE,
                "{}[{:02}:{:02}:{:02}.{:03}][{}][{}] ",
                "\x1b[32m",
                1u,
                2u,
                3u,
                456u,
                'I',
                "TestTag"
            );
            *r.out = '\0';
            anti_optimize(buf);
        }
    });
    report("logger header (7 args)", snp, fmtt);
}

TEST(BenchLogger, Hex8Bytes) {
    char buf[BUF_SIZE];
    const uint8_t data[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0xff, 0x10};
    const uint32_t snp = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            int n = std::snprintf(buf, BUF_SIZE, "%04x: ", static_cast<unsigned>(i));
            for (size_t j = 0; j < sizeof(data); ++j) {
                n += std::snprintf(buf + n, BUF_SIZE - n, "%02x ", data[j]);
            }
            anti_optimize(buf);
        }
    });
    const uint32_t fmtt = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            auto out = fmt::format_to_n(buf, BUF_SIZE, "{:04x}: ", i).out;
            for (size_t j = 0; j < sizeof(data); ++j) {
                out = fmt::format_to_n(out, BUF_SIZE, "{:02x} ", data[j]).out;
            }
            anti_optimize(buf);
        }
    });
    report("hexdump row (offset+8 bytes)", snp, fmtt);
}

TEST(BenchLogger, LongBody200Chars) {
    char buf[512];
    const std::string body(200, 'x');
    const uint32_t snp = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            std::snprintf(buf, sizeof(buf), "%s", body.c_str());
            anti_optimize(buf);
        }
    });
    const uint32_t fmtt = time_ns([&] {
        for (int i = 0; i < N_ITERS; ++i) {
            auto r = fmt::format_to_n(buf, sizeof(buf), "{}", body);
            *r.out = '\0';
            anti_optimize(buf);
        }
    });
    report("long body (200 chars)", snp, fmtt);
}

int main(int argc, char** argv) {
    std::printf("\n=== snprintf vs fmt format-to-buffer benchmark (N=%d) ===\n", N_ITERS);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
