/**
 * @file obtain_stage.h
 * @brief Stage that runs PD negotiation and learns PDO list
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <variant>

#include <tempo/bus/publisher.h>
#include <tempo/bus/visit.h>
#include <tempo/core/time.h>

#include "AP33772_debug.h"
#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/preferences_store.h"

namespace pocketpd {

    /**
     * @brief Validate the saved profile against the PDO at its saved index and
     * copy it into `out` ready to apply. PPS current is clamped to the PDO's
     * max, and fixed current stays 0.
     *
     * @return false if restore is off, the index is gone, or the profile there
     * no longer matches (out untouched).
     */
    inline bool resolve_profile(
        const Preferences& prefs, const PdSinkController& sink, LastProfile& out
    ) {
        if (!prefs.restore_last_profile_enabled) {
            return false;
        }

        const LastProfile& saved = prefs.last_profile;
        const int index = static_cast<int>(saved.pdo_index);
        if (index < 0 || index >= sink.pdo_count()) {
            return false;
        }

        const int saved_mv = static_cast<int>(saved.voltage_mv);
        bool matches = false;
        if (saved.is_pps) {
            auto min_mv = sink.pdo_min_voltage_mv(index);
            auto max_mv = sink.pdo_max_voltage_mv(index);
            matches = sink.is_index_pps(index) && saved_mv >= min_mv && saved_mv <= max_mv;
        } else {
            matches = sink.is_index_fixed(index) && sink.pdo_max_voltage_mv(index) == saved_mv;
        }

        if (!matches) {
            return false;
        }

        out = saved;
        if (saved.is_pps) {
            out.current_ma = static_cast<uint16_t>(
                std::min(static_cast<int>(saved.current_ma), sink.pdo_max_current_ma(index))
            );
        }
        return true;
    }

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

            LastProfile resolved;
            if (resolve_profile(m_prefs.get(), m_pd_sink, resolved)) {
                conductor.replace<NormalStage>(
                    resolved.pdo_index, resolved.voltage_mv, resolved.current_ma
                );
            }
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            // Periodically dump the PDO list for debugging
            if (m_dump_timer.tick(now_ms)) {
                dump_pdo_list();
            }

            if (m_timeout.reached(now_ms)) {
                conductor.reset_path<NormalStage, MenuStage, ProfilePickerStage>();
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {

            auto handler = tempo::overloaded{
                [&](const ButtonEvent& evt) {
                    if (evt.gesture == Gesture::SHORT) {
                        conductor.reset_path<NormalStage, MenuStage, ProfilePickerStage>();
                    }
                },
                [&](const EncoderEvent& evt) {
                    if (evt.delta != 0) {
                        conductor.reset_path<NormalStage, MenuStage, ProfilePickerStage>();
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
