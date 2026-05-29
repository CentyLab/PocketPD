/**
 * @file preferences_store.h
 * @brief Application-level wrapper around the `Preferences` POD + `Eeprom` HAL.
 *
 */
#pragma once

#include "v2/hal/eeprom.h"

namespace pocketpd {

    class PreferencesStore {
    private:
        Preferences m_preferences;
        Eeprom& m_eeprom;
        bool m_dirty = false;

    public:
        explicit PreferencesStore(Eeprom& eeprom, Preferences initial = {})
            : m_preferences(initial), m_eeprom(eeprom) {}

        bool load() {
            if (m_eeprom.load(m_preferences)) {
                return true;
            }
            m_eeprom.save(m_preferences);
            return false;
        }

        bool commit() {
            if (!m_dirty) {
                return true;
            }

            if (!m_eeprom.save(m_preferences)) {
                return false;
            }
            
            m_dirty = false;
            return true;
        }

        bool dirty() const {
            return m_dirty;
        }

        // —— Per-field accessors

        bool skip_picker_on_boot() const {
            return m_preferences.skip_picker_on_boot;
        }

        void set_skip_picker_on_boot(bool v) {
            if (m_preferences.skip_picker_on_boot == v) {
                return;
            }

            m_preferences.skip_picker_on_boot = v;
            m_dirty = true;
        }

        bool voltage_comp_enabled() const {
            return m_preferences.voltage_comp_enabled;
        }

        void set_voltage_comp_enabled(bool v) {
            if (m_preferences.voltage_comp_enabled == v) {
                return;
            }

            m_preferences.voltage_comp_enabled = v;
            m_dirty = true;
        }
    };

} // namespace pocketpd
