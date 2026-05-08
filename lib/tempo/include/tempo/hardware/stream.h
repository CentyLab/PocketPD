/**
 * @file stream.h
 * @brief Byte sink and source interfaces.
 */
#pragma once
#include <cstddef>
#include <type_traits>

namespace tempo {

    class StreamWriter {
    public:
        virtual ~StreamWriter() = default;

        virtual size_t write(const char* data, size_t len) = 0;
        virtual void flush() {}

        /**
         * @brief Compile-time literal fast path.
         *
         * @return size_t amount of bytes written
         */
        template <size_t N, typename T>
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
        size_t write(T (&literal)[N]) {
            static_assert(
                std::is_same_v<T, const char>,
                "StreamWriter::write(arr) is the literal fast path and only accepts "
                "string literals (const char[N]). For mutable char buffers or binary "
                "data, call write(data, len) with an explicit length."
            );
            return write(literal, N - 1); // strip null terminator
        }
    };

    class StreamReader {
    public:
        virtual ~StreamReader() = default;

        /**
         * @brief Bytes ready to read
         *
         * @return Number of buffered bytes, or 0 when empty.
         */
        virtual int available() = 0;

        /**
         * @brief Pop one byte from the source.
         *
         * @return The next byte as 0..255, or -1 if no byte is available.
         */
        virtual int read() = 0;
    };

} // namespace tempo
