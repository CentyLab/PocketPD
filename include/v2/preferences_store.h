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

        Preferences get() const {
            return m_preferences;
        }

        void set(const Preferences& v) {
            if (m_preferences == v) {
                return;
            }

            m_preferences = v;
            m_dirty = true;
        }
    };

} // namespace pocketpd
