/**
 * @file settings_stage.h
 * @brief Boolean settings list stage.
 *
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/preferences_store.h"
#include "v2/events.h"

namespace pocketpd {

    class SettingsStage : public App::Stage, public App::UseLog<SettingsStage> {
    private:
        using Display = tempo::Display;

        enum class Item : uint8_t { SKIP_PICKER = 0, VOLTAGE_COMP = 1 };

        static constexpr std::array<const char*, 2> ITEM_LABELS = {
            "Skip picker",
            "Voltage comp",
        };

        Display& m_display;
        PreferencesStore& m_prefs;
        uint8_t m_cursor = 0;

        int count() const {
            return ITEM_LABELS.size();
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
            switch (static_cast<Item>(m_cursor)) {
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
            m_display.clear();

            std::array<char, 32> buffer{};
            for (int i = 0; i < count(); ++i) {
                const int y = 12 * (i + 1);
                if (i == m_cursor) {
                    m_display.draw_text(0, y, ">");
                }

                const char mark = value_at(static_cast<Item>(i)) ? 'X' : ' ';
                std::snprintf(buffer.data(), buffer.size(), "[%c] %s", mark, ITEM_LABELS[i]);
                m_display.draw_text(10, y, buffer.data());
            }

            m_display.flush();
        }

    public:
        static constexpr const char* LOG_TAG = "Settings";

        SettingsStage(Display& display, PreferencesStore& prefs)
            : m_display(display), m_prefs(prefs) {}

        const char* name() const override {
            return "SETTINGS";
        }

        void on_enter(Conductor&, uint32_t) override {
            m_cursor = 0;
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
                    if (evt.delta == 0) return;
                    const int max_idx = count() - 1;
                    int next = std::clamp(static_cast<int>(m_cursor) + evt.delta, 0, max_idx);
                    if (next == m_cursor) return;
                    m_cursor = static_cast<uint8_t>(next);
                    draw();
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.request<MenuStage>();
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
