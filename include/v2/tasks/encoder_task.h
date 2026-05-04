/**
 * @file encoder_task.h
 * @brief Samples encoder at 5 ms; publishes signed delta on change. `on_start()` latches the
 * baseline once the scheduler goes live. `poll()` is public so native tests drive the task
 * directly.
 */
#pragma once

#include <tempo/bus/publisher.h>
#include <tempo/hardware/encoder_input.h>

#include "v2/app.h"
#include "v2/events.h"

namespace pocketpd {

    class EncoderTask : public App::BackgroundTask,
                        public tempo::UseLog<EncoderTask>,
                        public App::UsePublisher<EncoderTask> {
    private:
        tempo::EncoderInput& m_input;
        int m_last_position = 0;

        static constexpr int POLL_PERIOD_MS = 5;

    public:
        static constexpr const char* LOG_TAG = "EncoderTask";

        explicit EncoderTask(tempo::EncoderInput& input)
            : App::BackgroundTask(POLL_PERIOD_MS), m_input(input) {}

        const char* name() const override {
            return "EncoderTask";
        }

        void on_start() override {
            m_last_position = m_input.position();
        }

        void poll() {
            const int pos = m_input.position();
            const int delta = pos - m_last_position;
            if (delta != 0) {
                publish(EncoderEvent{delta});
                m_last_position = pos;
            }
        }

    protected:
        void on_tick(uint32_t) override {
            poll();
        }
    };

} // namespace pocketpd
