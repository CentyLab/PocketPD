#pragma once

#include <gmock/gmock.h>
#include <tempo/hardware/stream.h>

#include <cstddef>
#include <queue>
#include <string>

namespace pocketpd {

    class MockStreamReader : public tempo::StreamReader {
    public:
        MOCK_METHOD(int, available, (), (override));
        MOCK_METHOD(int, read, (), (override));
    };

    /**
     * @brief Scripted StreamReader. `push(...)` enqueues bytes to be popped
     * one at a time by `read()`. `available()` reports the queue size.
     */
    class FakeStreamReader : public tempo::StreamReader {
    private:
        std::queue<unsigned char> m_bytes;

    public:
        void push(const char* data, size_t len) {
            for (size_t i = 0; i < len; ++i) {
                m_bytes.push(static_cast<unsigned char>(data[i]));
            }
        }

        void push(const std::string& s) {
            push(s.data(), s.size());
        }

        int available() override {
            return static_cast<int>(m_bytes.size());
        }

        int read() override {
            if (m_bytes.empty()) {
                return -1;
            }
            const int byte = m_bytes.front();
            m_bytes.pop();
            return byte;
        }
    };

} // namespace pocketpd
