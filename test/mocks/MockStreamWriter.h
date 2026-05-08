#pragma once

#include <gmock/gmock.h>
#include <tempo/hardware/stream.h>

#include <cstddef>
#include <string>

namespace pocketpd {

    class MockStreamWriter : public tempo::StreamWriter {
    public:
        MOCK_METHOD(size_t, write, (const char* data, size_t len), (override));
        MOCK_METHOD(void, flush, (), (override));
    };

    /**
     * @brief Recording StreamWriter. Appends every write to `captured` so tests
     * can assert on log / echo output.
     */
    class CapturingStreamWriter : public tempo::StreamWriter {
    public:
        std::string captured;
        size_t write_calls = 0;
        size_t flush_calls = 0;

        size_t write(const char* data, size_t len) override {
            captured.append(data, len);
            ++write_calls;
            return len;
        }

        void flush() override {
            ++flush_calls;
        }

        void clear() {
            captured.clear();
            write_calls = 0;
            flush_calls = 0;
        }
    };

} // namespace pocketpd
