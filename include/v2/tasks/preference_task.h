/**
 * @file preference_task.h
 * @brief Owns runtime updates to PreferencesStore and debounced saves to flash.
 */
#pragma once

#include <cstdint>
#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/pocketpd.h"
#include "v2/preferences_store.h"

namespace pocketpd {

    /**
     * @brief Applies runtime preference updates via events and persists them.
     *
     */
    class PreferenceTask : public App::PeriodicTask, public App::UseLog<PreferenceTask> {
    private:
        PreferencesStore& m_prefs;
        Preferences m_last_seen{};
        uint32_t m_stamp_ms = 0;

    public:
        static constexpr uint32_t PERIOD_MS = 250;
        static constexpr const char* LOG_TAG = "Prefs";

        explicit PreferenceTask(PreferencesStore& prefs)
            : App::PeriodicTask(PERIOD_MS), m_prefs(prefs) {}

        const char* name() const override {
            return LOG_TAG;
        }

        void on_event(const Event& event, uint32_t) override {
            const auto* profile = std::get_if<ActiveProfileEvent>(&event);
            if (profile == nullptr || profile->pdo_index < 0) {
                return;
            }

            Preferences prefs = m_prefs.get();
            if (!prefs.restore_last_profile_enabled) {
                return;
            }

            prefs.last_profile = {
                profile->is_pps,
                static_cast<uint16_t>(profile->mv),
                static_cast<uint16_t>(profile->ma),
                static_cast<uint8_t>(profile->pdo_index),
            };

            m_prefs.set(prefs);
        }

        void on_tick(uint32_t now_ms) override {
            const Preferences current = m_prefs.get();
            if (current != m_last_seen) {
                m_last_seen = current;
                m_stamp_ms = now_ms;
                return;
            }

            if (!m_prefs.dirty() || now_ms - m_stamp_ms < EEPROM_SAVE_DEBOUNCE_MS) {
                return;
            }

            if (!m_prefs.commit()) {
                log.error("preference save failed");
            }
        }
    };

} // namespace pocketpd
