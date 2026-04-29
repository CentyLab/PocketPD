/**
 * @file Stream.h
 * @brief Stream interface for reading and writing bytes.
 */
#pragma once
#include <cstddef>
#include <cstring>

namespace tempo {

    class StreamReader {
    public:
        StreamReader() = default;
        virtual ~StreamReader() = default;

        virtual size_t available() const = 0;
        virtual size_t read(const char* data, size_t len) = 0;
        virtual size_t read(const char* data) {
            return read(data, strlen(data));
        };
    };

    class StreamWriter {
    public:
        StreamWriter() = default;
        virtual ~StreamWriter() = default;

        virtual size_t write(const char* data, size_t len) = 0;
        virtual size_t write(const char* data) {
            return write(data, strlen(data));
        };
    };

    class Stream : public StreamWriter, StreamReader {
    public:
    };

}; // namespace tempo
