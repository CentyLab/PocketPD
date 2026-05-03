/**
 * @file encoder_task.h
 * @brief Samples encoder at 5 ms; publishes signed delta on change. First poll arms the baseline
 * silently. `poll()` is public so native tests drive the task directly.
 */
#pragma once

#include <tempo/bus/publisher.h>

#include <cstdint>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/encoder_input.h"

namespace pocketpd {

    class EncoderTask : public App::BackgroundTask, public tempo::UseLog<EncoderTask> {
    public:
        static constexpr uint32_t POLL_PERIOD_MS = 5;
        static constexpr const char* LOG_TAG = "EncoderTask";

    private:
        EncoderInput& m_input;
        tempo::Publisher<Event>& m_publisher;
        int32_t m_last_position = 0;
        bool m_armed = false;

    public:
        EncoderTask(EncoderInput& input, tempo::Publisher<Event>& publisher)
            : App::BackgroundTask(POLL_PERIOD_MS), m_input(input), m_publisher(publisher) {}

        const char* name() const override {
            return "EncoderTask";
        }

        void poll() {
            const auto pos = m_input.position();
            log.debug("%d", pos);
            if (!m_armed) {
                m_last_position = pos;
                m_armed = true;
                return;
            }
            const auto delta = pos - m_last_position;
            if (delta != 0) {
                m_publisher.publish(EncoderEvent{static_cast<int16_t>(delta)});
                m_last_position = pos;
            }
        }

    protected:
        void on_tick(uint32_t) override {
            poll();
        }
    };

} // namespace pocketpd
