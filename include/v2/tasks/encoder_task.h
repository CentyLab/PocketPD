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
#include "v2/preferences_store.h"

namespace pocketpd {

    class EncoderTask : public App::BackgroundTask,
                        public App::UseLog<EncoderTask>,
                        public App::UsePublisher<EncoderTask> {
    private:
        tempo::EncoderInput& m_input;
        const PreferencesStore& m_prefs;
        int m_last_position = 0;

        static constexpr int POLL_PERIOD_MS = 25;

    public:
        static constexpr const char* LOG_TAG = "EncoderTask";

        EncoderTask(tempo::EncoderInput& input, const PreferencesStore& prefs)
            : App::BackgroundTask(POLL_PERIOD_MS), m_input(input), m_prefs(prefs) {}

        const char* name() const override {
            return "EncoderTask";
        }

        void on_start() override {
            m_last_position = m_input.position();
        }

        void poll() {
            const int pos = m_input.position();
            int delta = pos - m_last_position;
            if (delta != 0) {
                // Flipped display turns the knob around with the unit, so CW now reads as CCW.
                if (m_prefs.get().flip_display) {
                    delta = -delta;
                }
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
