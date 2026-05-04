/**
 * @file button_task.h
 * @brief Polls 3 buttons at 5 ms and feeds each into its own ButtonGestureDetector. Publishes a
 * `ButtonEvent` whenever a detector recognizes a SHORT or LONG gesture. `poll(now_ms)`
 * is public so native tests drive the task directly.
 */
#pragma once

#include <tempo/bus/publisher.h>
#include <tempo/hardware/button_input.h>

#include <cstdint>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/input/button_gesture.h"
#include "v2/state.h"

namespace pocketpd {

    class ButtonTask : public App::BackgroundTask, public tempo::UseLog<ButtonTask> {
    private:
        struct DetectorRef {
            ButtonId id;
            tempo::ButtonInput* input;
            ButtonGestureDetector detector;

            DetectorRef() = delete;
        };

        tempo::Publisher<Event>& m_publisher;
        std::array<DetectorRef, 3> m_detectors;

        static constexpr uint32_t POLL_PERIOD_MS = 5;

    public:
        static constexpr const char* LOG_TAG = "ButtonTask";

        ButtonTask(
            tempo::Publisher<Event>& publisher,
            tempo::ButtonInput& btn_encoder,
            tempo::ButtonInput& btn_vi_selector,
            tempo::ButtonInput& btn_output,
            ButtonGestureConfig gesture_config = {}
        )
            : App::BackgroundTask(POLL_PERIOD_MS),
              m_publisher(publisher),
              m_detectors{
                  DetectorRef{
                      ButtonId::ENCODER,
                      &btn_encoder,
                      ButtonGestureDetector{gesture_config},
                  },
                  DetectorRef{
                      ButtonId::SELECT_VI,
                      &btn_vi_selector,
                      ButtonGestureDetector{gesture_config},
                  },
                  DetectorRef{
                      ButtonId::OUTPUT_TOGGLE,
                      &btn_output,
                      ButtonGestureDetector{gesture_config},
                  },
              } {}

        const char* name() const override {
            return "ButtonTask";
        }

        const char* button_name(ButtonId id) {
            switch (id) {
            case ButtonId::ENCODER:
                return "ENCODER";
            case ButtonId::SELECT_VI:
                return "SELECT_VI";
            case ButtonId::OUTPUT_TOGGLE:
                return "OUTPUT_TOGGLE";
            default:
                return "UNKNOWN";
            }
        }

        void poll(uint32_t now_ms) {
            for (DetectorRef& ref : m_detectors) {
                ref.input->update();

                const bool is_held = ref.input->is_held();
                const std::optional<Gesture> gesture = ref.detector.update(is_held, now_ms);
                if (gesture.has_value()) {
                    log.debug(
                        "button=%s detected gesture=%s",
                        button_name(ref.id),
                        gesture.value() == Gesture::SHORT ? "SHORT" : "LONG"
                    );
                    m_publisher.publish(ButtonEvent{ref.id, gesture.value()});
                }
            }
        }

    protected:
        void on_tick(uint32_t now_ms) override {
            poll(now_ms);
        }
    };

} // namespace pocketpd
