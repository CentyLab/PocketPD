/**
 * @file settings_stage.h
 * @brief Boolean settings list stage.
 *
 */
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/display_orientation.h"
#include "v2/preferences_store.h"
#include "v2/ui/table_view.h"

namespace pocketpd {

    class SettingsStage : public App::Stage, public App::UseLog<SettingsStage> {
    private:
        using Display = tempo::Display;

        enum class Item : uint8_t {
            RESTORE_PROFILE,
            VOLTAGE_COMPENSATE,
            FLIP_DISPLAY,
        };

        struct SettingItem {
            Item item;
            const char* label;
        };

        static constexpr std::array<SettingItem, 3> ITEMS = {{
            {Item::RESTORE_PROFILE, "Restore profile"},
            {Item::VOLTAGE_COMPENSATE, "Voltage comp"},
            {Item::FLIP_DISPLAY, "Flip display"},
        }};

        Display& m_display;
        DisplayOrientation& m_orientation;
        PreferencesStore& m_prefs;
        TableView m_table{};

        int count() const {
            return ITEMS.size();
        }

        bool value_at(Item item) const {
            const Preferences prefs = m_prefs.get();
            switch (item) {
            case Item::RESTORE_PROFILE:
                return prefs.restore_last_profile_enabled;
            case Item::VOLTAGE_COMPENSATE:
                return prefs.voltage_compensate_enabled;
            case Item::FLIP_DISPLAY:
                return prefs.flip_display_enabled;
            }
            return false;
        }

        void toggle_current() {
            Preferences prefs = m_prefs.get();
            switch (ITEMS[m_table.cursor()].item) {
            case Item::RESTORE_PROFILE:
                prefs.restore_last_profile_enabled = !prefs.restore_last_profile_enabled;
                break;
            case Item::VOLTAGE_COMPENSATE:
                prefs.voltage_compensate_enabled = !prefs.voltage_compensate_enabled;
                break;
            case Item::FLIP_DISPLAY:
                prefs.flip_display_enabled = !prefs.flip_display_enabled;
                m_orientation.set_flipped(prefs.flip_display_enabled);
                break;
            }
            m_prefs.set(prefs);

            draw();
        }

        void draw() {
            std::array<std::array<char, 32>, ITEMS.size()> buffers{};
            std::array<TableRow, ITEMS.size()> rows{};
            for (size_t i = 0; i < ITEMS.size(); ++i) {
                const char mark = value_at(ITEMS[i].item) ? 'X' : ' ';
                std::snprintf(
                    buffers[i].data(), buffers[i].size(), "[%c] %s", mark, ITEMS[i].label
                );
                rows[i].text = buffers[i].data();
            }

            TableModel model;
            model.rows = rows.data();
            model.count = static_cast<uint8_t>(ITEMS.size());
            m_table.render(m_display, model);
        }

    public:
        SettingsStage(Display& display, DisplayOrientation& orientation, PreferencesStore& prefs)
            : m_display(display), m_orientation(orientation), m_prefs(prefs) {}

        void on_enter(Conductor&, uint32_t) override {
            m_table.reset();
            draw();
        }

        void on_exit(Conductor&, uint32_t) override {
            if (!m_prefs.commit()) {
                log.error("settings save failed");
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    if (evt.delta == 0) {
                        return;
                    }
                    if (m_table.move(evt.delta, static_cast<uint8_t>(count()))) {
                        draw();
                    }
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.pop();
                        return;
                    }
                    if (evt.id == ButtonId::ENCODER && evt.gesture == Gesture::LONG) {
                        toggle_current();
                    }
                },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }
    };

} // namespace pocketpd
