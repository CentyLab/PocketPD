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
#include "v2/preferences_store.h"
#include "v2/ui/table_view.h"

namespace pocketpd {

    class SettingsStage : public App::Stage, public App::UseLog<SettingsStage> {
    private:
        using Display = tempo::Display;

        enum class Item : uint8_t {
            SKIP_PICKER,
            VOLTAGE_COMP,
        };

        struct SettingItem {
            Item item;
            const char* label;
        };

        static constexpr std::array<SettingItem, 2> ITEMS = {{
            {Item::SKIP_PICKER, "Skip picker"},
            {Item::VOLTAGE_COMP, "Voltage comp"},
        }};

        Display& m_display;
        PreferencesStore& m_prefs;
        TableView m_table{};

        int count() const {
            return ITEMS.size();
        }

        bool value_at(Item item) const {
            switch (item) {
            case Item::SKIP_PICKER:
                return m_prefs.skip_picker_on_boot();
            case Item::VOLTAGE_COMP:
                return m_prefs.voltage_comp_enabled();
            }
            return false;
        }

        void toggle_current() {
            switch (ITEMS[m_table.cursor()].item) {
            case Item::SKIP_PICKER:
                m_prefs.set_skip_picker_on_boot(!m_prefs.skip_picker_on_boot());
                break;
            case Item::VOLTAGE_COMP:
                m_prefs.set_voltage_comp_enabled(!m_prefs.voltage_comp_enabled());
                break;
            }

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
        SettingsStage(Display& display, PreferencesStore& prefs)
            : m_display(display), m_prefs(prefs) {}

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
