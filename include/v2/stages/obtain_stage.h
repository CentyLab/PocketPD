/**
 * @file obtain_stage.h
 * @brief Stage that runs PD negotiation and learns PDO list
 */
#pragma once

#include <tempo/bus/publisher.h>
#include <tempo/bus/visit.h>
#include <tempo/core/time.h>

#include <array>
#include <cstdint>
#include <variant>

#include "AP33772_debug.h"
#include "v2/app.h"
#include "v2/preferences_store.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class ObtainStage : public App::Stage,
                        public App::UseLog<ObtainStage>,
                        public App::UsePublisher<ObtainStage> {
    private:
        PdSinkController& m_pd_sink;
        const PreferencesStore& m_prefs;

        tempo::IntervalTimer m_dump_timer{500};
        tempo::TimeoutTimer m_timeout;

        void dump_pdo_list() {
            std::array<char, 1024> buffer{};
            ap33772::format_pdo(m_pd_sink, buffer.data(), buffer.size());
            log.info(buffer.data());
        }

    public:
        static constexpr const char* LOG_TAG = "Obtain";

        ObtainStage(PdSinkController& pd_sink, const PreferencesStore& prefs)
            : m_pd_sink(pd_sink), m_prefs(prefs) {}

        const char* name() const override {
            return "OBTAIN";
        }

        void on_enter(Conductor& conductor, uint32_t now_ms) override {
            m_timeout.set(now_ms, OBTAIN_TO_PROFILE_PICKER_MS);

            if (!m_pd_sink.begin()) {
                log.error("PD negotiation failed");
            }

            if (m_prefs.skip_picker_on_boot()) {
                conductor.request<NormalStage>();
            }
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            // Periodically dump the PDO list for debugging
            if (m_dump_timer.tick(now_ms)) {
                dump_pdo_list();
            }

            if (m_timeout.reached(now_ms)) {
                conductor.request<ProfilePickerStage>();
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {

            auto handler = tempo::overloaded{
                [&](const ButtonEvent& evt) {
                    if (evt.gesture == Gesture::SHORT) {
                        conductor.request<ProfilePickerStage>();
                    }
                },
                [&](const EncoderEvent& evt) {
                    if (evt.delta != 0) {
                        conductor.request<ProfilePickerStage>();
                    }
                },
                [](const auto&) {
                    // handle std::monostate and any unrelated event variants
                },
            };

            std::visit(handler, event);
        }
    };

} // namespace pocketpd
