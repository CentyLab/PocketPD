/**
 * On-device snprintf vs fmt::format_to_n benchmark.
 *
 * Build/flash:
 *   pio run -e bench_logger -t upload
 *   pio device monitor -e bench_logger
 *
 * Runs N iterations of each format path into a stack buffer (no Serial in the
 * timed region) for representative payloads, prints ns-per-op + ratio, then halts.
 */
#include <Arduino.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <tempo/diag/stream_iterator.h>
#include <tempo/hardware/stream.h>

#pragma push_macro("F")
#ifdef F
#undef F
#endif
#include <fmt/format.h>
#pragma pop_macro("F")

namespace {

    constexpr int N_ITERS = 5000;
    constexpr size_t BUF_SIZE = 256;

    inline void anti_optimize(const char* p) {
        asm volatile("" : : "r"(p) : "memory");
    }

    class NullStreamWriter : public tempo::StreamWriter {
    public:
        size_t bytes = 0;
        size_t calls = 0;
        size_t write(const char* data, size_t len) override {
            bytes += len;
            ++calls;
            anti_optimize(data);
            return len;
        }
    };

    template <typename Fn>
    uint32_t time_us(Fn&& fn) {
        const uint32_t t0 = micros();
        fn();
        return micros() - t0;
    }

    void report(const char* label, uint32_t snp_us, uint32_t fmt_us) {
        const float snp_per = (snp_us * 1000.0f) / N_ITERS;
        const float fmt_per = (fmt_us * 1000.0f) / N_ITERS;
        const float ratio = fmt_per / snp_per;
        char line[160];
        std::snprintf(
            line,
            sizeof(line),
            "  %-32s snprintf=%7.1f ns/op  fmt=%7.1f ns/op  ratio=%.2fx\r\n",
            label,
            static_cast<double>(snp_per),
            static_cast<double>(fmt_per),
            static_cast<double>(ratio)
        );
        Serial.write(line);
    }

    void bench_single_uint32() {
        char buf[BUF_SIZE];
        const uint32_t snp = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                std::snprintf(buf, BUF_SIZE, "v=%u", static_cast<unsigned>(i));
                anti_optimize(buf);
            }
        });
        const uint32_t fmtt = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                auto r = fmt::format_to_n(buf, BUF_SIZE, "v={}", i);
                *r.out = '\0';
                anti_optimize(buf);
            }
        });
        report("single uint32", snp, fmtt);
    }

    void bench_multi_args() {
        char buf[BUF_SIZE];
        const uint32_t snp = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                std::snprintf(
                    buf, BUF_SIZE, "PD ready: %u PDO (%u PPS) %s", 7u, 3u, "Anker"
                );
                anti_optimize(buf);
            }
        });
        const uint32_t fmtt = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                auto r = fmt::format_to_n(
                    buf, BUF_SIZE, "PD ready: {} PDO ({} PPS) {}", 7, 3, "Anker"
                );
                *r.out = '\0';
                anti_optimize(buf);
            }
        });
        report("multi-arg (2 int + str)", snp, fmtt);
    }

    void bench_logger_header() {
        char buf[BUF_SIZE];
        const uint32_t snp = time_us([&] {
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
        const uint32_t fmtt = time_us([&] {
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

    void bench_hexdump_row() {
        char buf[BUF_SIZE];
        const uint8_t data[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0xff, 0x10};
        const uint32_t snp = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                int n = std::snprintf(buf, BUF_SIZE, "%04x: ", static_cast<unsigned>(i));
                for (size_t j = 0; j < sizeof(data); ++j) {
                    n += std::snprintf(buf + n, BUF_SIZE - n, "%02x ", data[j]);
                }
                anti_optimize(buf);
            }
        });
        const uint32_t fmtt = time_us([&] {
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

    void bench_long_body() {
        char buf[512];
        char body[201];
        std::memset(body, 'x', 200);
        body[200] = '\0';
        const uint32_t snp = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                std::snprintf(buf, sizeof(buf), "%s", body);
                anti_optimize(buf);
            }
        });
        const uint32_t fmtt = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                auto r = fmt::format_to_n(buf, sizeof(buf), "{}", static_cast<const char*>(body));
                *r.out = '\0';
                anti_optimize(buf);
            }
        });
        report("long body (200 chars)", snp, fmtt);
    }

    void bench_sink_short_line() {
        NullStreamWriter sw;
        const uint32_t t = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                tempo::BufferedStreamSink sink(sw);
                fmt::format_to(
                    sink.out(), "v={} ma={} idx={}", 5000u, 1000u, static_cast<uint32_t>(i)
                );
            }
        });
        char line[160];
        std::snprintf(
            line,
            sizeof(line),
            "  %-32s sink=%7.1f ns/op  bytes=%u  writes=%u  buf=%uB\r\n",
            "sink short line (3 args)",
            static_cast<double>((t * 1000.0f) / N_ITERS),
            static_cast<unsigned>(sw.bytes),
            static_cast<unsigned>(sw.calls),
            static_cast<unsigned>(tempo::BufferedStreamSink::BUF_SIZE)
        );
        Serial.write(line);
    }

    void bench_sink_logger_header_like() {
        NullStreamWriter sw;
        const uint32_t t = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                tempo::BufferedStreamSink sink(sw);
                fmt::format_to(
                    sink.out(),
                    "{}[{:02}:{:02}:{:02}.{:03}][{}][{}] body",
                    "\x1b[32m",
                    1u,
                    2u,
                    3u,
                    456u,
                    'I',
                    "TestTag"
                );
            }
        });
        char line[160];
        std::snprintf(
            line,
            sizeof(line),
            "  %-32s sink=%7.1f ns/op  bytes=%u  writes=%u  buf=%uB\r\n",
            "sink full log line (~38 ch)",
            static_cast<double>((t * 1000.0f) / N_ITERS),
            static_cast<unsigned>(sw.bytes),
            static_cast<unsigned>(sw.calls),
            static_cast<unsigned>(tempo::BufferedStreamSink::BUF_SIZE)
        );
        Serial.write(line);
    }

    void bench_sink_long_body() {
        NullStreamWriter sw;
        char body[201];
        std::memset(body, 'x', 200);
        body[200] = '\0';
        const uint32_t t = time_us([&] {
            for (int i = 0; i < N_ITERS; ++i) {
                tempo::BufferedStreamSink sink(sw);
                fmt::format_to(sink.out(), "{}", static_cast<const char*>(body));
            }
        });
        char line[160];
        std::snprintf(
            line,
            sizeof(line),
            "  %-32s sink=%7.1f ns/op  bytes=%u  writes=%u  buf=%uB\r\n",
            "sink long body (200 chars)",
            static_cast<double>((t * 1000.0f) / N_ITERS),
            static_cast<unsigned>(sw.bytes),
            static_cast<unsigned>(sw.calls),
            static_cast<unsigned>(tempo::BufferedStreamSink::BUF_SIZE)
        );
        Serial.write(line);
    }

} // namespace

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    delay(2000); // give the host monitor time to attach
}

void loop() {
    Serial.write("\r\n=== RP2040 snprintf vs fmt format-to-buffer benchmark ===\r\n");
    char hdr[80];
    std::snprintf(hdr, sizeof(hdr), "  N=%d, F_CPU=%lu Hz\r\n", N_ITERS, F_CPU);
    Serial.write(hdr);

    bench_single_uint32();
    bench_multi_args();
    bench_logger_header();
    bench_hexdump_row();
    bench_long_body();
    bench_sink_short_line();
    bench_sink_logger_header_like();
    bench_sink_long_body();

    Serial.write("=== bench done; sleeping 5s ===\r\n");
    Serial.flush();
    delay(5000);
}
