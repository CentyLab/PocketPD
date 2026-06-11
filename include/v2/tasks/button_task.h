/**
 * @file button_task.h
 * @brief Run guesture recognition on 3 buttons at 5 ms interval.
 */
#pragma once

#include <tempo/bus/publisher.h>
#include <tempo/hardware/button_input.h>

#include <cstdint>
#include <optional>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/input/button_gesture.h"
#include "v2/input/two_buttons_gesture.h"
#include "v2/pocketpd.h"
#include "v2/preferences_store.h"

namespace pocketpd {

    class ButtonTask : public App::BackgroundTask,
                       public App::UseLog<ButtonTask>,
                       public App::UsePublisher<ButtonTask> {
    private:
        struct DetectorRef {
            ButtonId id;
            tempo::ButtonInput* input;
            ButtonGestureDetector detector;

            DetectorRef() = delete;
        };
        
        std::array<DetectorRef, 3> m_detector_refs;
        TwoButtonsGestureDetector m_combo_detector;
        const PreferencesStore& m_prefs;

        static constexpr uint32_t POLL_PERIOD_MS = 5;
        static constexpr size_t IDX_ENCODER = 0;
        static constexpr size_t IDX_L = 1;
        static constexpr size_t IDX_R = 2;

        /**
         * @brief Flipped display means the unit is held upside down, so the physical L button
         * sits on the user's right. Swap L/R at publish time; ENCODER and L_R are symmetric.
         */
        ButtonId published_id(ButtonId id) const {
            if (!m_prefs.get().flip_display) {
                return id;
            }
            switch (id) {
            case ButtonId::L:
                return ButtonId::R;
            case ButtonId::R:
                return ButtonId::L;
            default:
                return id;
            }
        }

    public:
        static constexpr const char* LOG_TAG = "ButtonTask";

        ButtonTask(
            tempo::ButtonInput& btn_encoder,
            tempo::ButtonInput& btn_l,
            tempo::ButtonInput& btn_r,
            const PreferencesStore& prefs,
            ButtonGestureConfig gesture_config = {}
        )
            : App::BackgroundTask(POLL_PERIOD_MS),
              m_detector_refs{
                  DetectorRef{
                      ButtonId::ENCODER,
                      &btn_encoder,
                      ButtonGestureDetector{gesture_config},
                  },
                  DetectorRef{
                      ButtonId::L,
                      &btn_l,
                      ButtonGestureDetector{gesture_config},
                  },
                  DetectorRef{
                      ButtonId::R,
                      &btn_r,
                      ButtonGestureDetector{gesture_config},
                  },
              },
              m_combo_detector(gesture_config),
              m_prefs(prefs) {}

        const char* name() const override {
            return "ButtonTask";
        }

        void poll(uint32_t now_ms) {
            // Update each button detector and capture the gesture.
            std::array<std::optional<Gesture>, 3> gestures{};

            for (size_t i = 0; i < m_detector_refs.size(); ++i) {
                DetectorRef& ref = m_detector_refs[i];
                ref.input->update();
                gestures[i] = ref.detector.update(now_ms, ref.input->is_held());

                if (ref.detector.is_pressed()) {
                    log.debug("button={} press_start_at={}ms", to_string(ref.id), now_ms);
                }
            }

            // Run the combo detector. It stays active until both buttons are released. The combo
            // active state suppresses L/R singles.
            const auto combo_gesture = m_combo_detector.update(
                now_ms,
                m_detector_refs[IDX_L].detector.is_holding(),
                m_detector_refs[IDX_R].detector.is_holding()
            );

            // If the combo detector fired, publish the combo gesture.
            if (combo_gesture.has_value()) {
                log.debug("combo={} gesture={}", to_string(ButtonId::L_R), "LONG");
                publish(ButtonEvent{ButtonId::L_R, combo_gesture.value()});
            }

            // Publish each button gesture that is not suppressed by the combo detector.
            for (size_t i = 0; i < m_detector_refs.size(); ++i) {
                DetectorRef& ref = m_detector_refs[i];
                const std::optional<Gesture>& gesture = gestures[i];

                if (!gesture.has_value()) {
                    continue;
                }

                // Suppress the gesture if it is an L or R single and the combo detector is active.
                const bool is_LR = (ref.id == ButtonId::L) || (ref.id == ButtonId::R);
                if (is_LR && m_combo_detector.is_active()) {
                    continue;
                }

                const uint32_t duration = ref.detector.duration(now_ms);
                const char* msg = "button={} gesture={} hold_duration={}";
                log.debug(msg, to_string(ref.id), to_string(gesture.value()), duration);
                publish(ButtonEvent{published_id(ref.id), gesture.value()});
            }
        }

    protected:
        void on_tick(uint32_t now_ms) override {
            poll(now_ms);
        }
    };

} // namespace pocketpd
