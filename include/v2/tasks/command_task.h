/**
 * @file command_task.h
 * @brief Polls a `tempo::StreamReader` for host-side serial input. On every
 * newline (`\n` or `\r`) the buffered bytes are echoed back through the
 * supplied `tempo::StreamWriter` followed by a single `\n`. Lines longer than
 * `MAX_MSG_LEN` bytes are echoed and reset on overflow.
 *
 * The buffer is kept null-terminated at all times so consumers can treat
 * `m_buffer.data()` as a C-string without carrying the length around.
 * `BUFFER_SIZE` is the array size; `MAX_MSG_LEN = BUFFER_SIZE - 1` is the
 * maximum payload length, leaving one byte reserved for the terminator.
 */
#pragma once

#include <tempo/hardware/stream.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "v2/app.h"

namespace pocketpd {

    class CommandTask : public App::BackgroundTask,
                        public App::UseLog<CommandTask>,
                        public App::UsePublisher<CommandTask> {
    private:
        static constexpr uint32_t POLL_PERIOD_MS = 10;
        static constexpr size_t BUFFER_SIZE = 128;
        static constexpr size_t MAX_MSG_LEN = BUFFER_SIZE - 1;

        tempo::StreamReader& m_source;
        tempo::StreamWriter& m_writer;
        
        std::array<char, BUFFER_SIZE> m_buffer{};
        size_t m_len = 0;

        /**
         * @brief Make sure we are always null terminated
         *
         */
        void terminate() {
            m_buffer[m_len] = '\0';
        }

        void echo_buffer() {
            terminate();
            log.debug("rx={} bytes", m_len);
            log.info(m_buffer.data());
            m_len = 0;
            terminate();
        }

    public:
        static constexpr const char* LOG_TAG = "CommandTask";

        CommandTask(tempo::StreamReader& source, tempo::StreamWriter& writer)
            : App::BackgroundTask(POLL_PERIOD_MS), m_source(source), m_writer(writer) {}

        const char* name() const override {
            return LOG_TAG;
        }

        void poll(uint32_t) {
            while (m_source.available() > 0) {
                const int next = m_source.read();
                if (next < 0) {
                    break;
                }
                const char ch = static_cast<char>(next);

                if (ch == '\n' || ch == '\r') {
                    if (m_len > 0) {
                        echo_buffer();
                    }
                    continue;
                }

                if (m_len >= MAX_MSG_LEN) {
                    log.warn("command overflow at {} bytes; flushing", MAX_MSG_LEN);
                    echo_buffer();
                }

                m_buffer[m_len++] = ch;
                terminate();
            }
        }

    protected:
        void on_tick(uint32_t now_ms) override {
            poll(now_ms);
        }
    };

} // namespace pocketpd
