/**
 * @file stream_iterator.h
 * @brief Buffered output sink that adapts a `tempo::StreamWriter` to fmt's output_iterator
 * concept.
 *
 * Used by `tempo::Logger` to stream `fmt::format_to(...)` output through `StreamWriter::write`
 * without per-character virtual dispatch. The sink owns a small stack buffer; characters
 * accumulate until the buffer fills (auto-flush) or the sink destructs (final flush).
 */
#pragma once

#include <array>
#include <cstddef>
#include <iterator>

#include "tempo/hardware/stream.h"

namespace tempo {

    /**
     * @brief Small fixed-buffer sink writing into a `StreamWriter` in chunks.
     *
     * Iterator obtained via `out()` satisfies fmt's output_iterator requirements;
     * `*it = c` calls `put(c)`. The sink owns the buffer; lifetime is one log call.
     */
    class BufferedStreamSink {
    public:
        // —— Tunables
        static constexpr size_t BUF_SIZE = 64;

    private:
        StreamWriter& m_sw;
        // Uninitialized: only bytes [0, m_n) are read by flush(); zero-init wastes cycles.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        std::array<char, BUF_SIZE> m_buf;
        size_t m_n = 0;

    public:
        explicit BufferedStreamSink(StreamWriter& sw) : m_sw(sw) {}
        ~BufferedStreamSink() {
            flush();
        }

        BufferedStreamSink(const BufferedStreamSink&) = delete;
        BufferedStreamSink& operator=(const BufferedStreamSink&) = delete;
        BufferedStreamSink(BufferedStreamSink&&) = delete;
        BufferedStreamSink& operator=(BufferedStreamSink&&) = delete;

        void put(char c) {
            if (m_n == BUF_SIZE) {
                flush();
            }
            m_buf[m_n++] = c;
        }

        void flush() {
            if (m_n != 0) {
                m_sw.write(m_buf.data(), m_n);
                m_n = 0;
            }
        }

        class Iterator {
            BufferedStreamSink* m_sink;

        public:
            using iterator_category = std::output_iterator_tag;
            using value_type = char;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = void;

            explicit Iterator(BufferedStreamSink& sink) : m_sink(&sink) {}

            Iterator& operator=(char c) {
                m_sink->put(c);
                return *this;
            }

            Iterator& operator*() {
                return *this;
            }
            Iterator& operator++() {
                return *this;
            }
            Iterator operator++(int) {
                return *this;
            }
        };

        Iterator out() {
            return Iterator(*this);
        }
    };

} // namespace tempo
