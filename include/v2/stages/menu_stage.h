/**
 * @file menu_stage.h
 * @brief Top-level menu stage.
 *
 */
#pragma once

#include <array>
#include <cstdint>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/ui/table_view.h"

namespace pocketpd {

    class MenuStage : public App::Stage, public App::UseLog<MenuStage> {
    private:
        using Display = tempo::Display;

        enum class Item : uint8_t {
            PROFILE_PICKER,
            SETTINGS,
        };

        struct MenuItem {
            Item item;
            const char* label;
        };

        static constexpr std::array<MenuItem, 2> ITEMS = {{
            {Item::PROFILE_PICKER, "Profile Picker"},
            {Item::SETTINGS, "Settings"},
        }};

        Display& m_display;
        TableView m_table;

        int count() const {
            return ITEMS.size();
        }

        void draw() {
            std::array<TableRow, ITEMS.size()> rows{};
            for (size_t i = 0; i < ITEMS.size(); ++i) {
                rows[i].text = ITEMS[i].label;
            }

            TableModel model;
            model.rows = rows.data();
            model.count = static_cast<uint8_t>(ITEMS.size());
            m_table.render(m_display, model);
        }

    public:
        explicit MenuStage(Display& display) : m_display(display) {}

        void on_enter(Conductor&, uint32_t) override {
            m_table.reset();
            draw();
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
                        switch (ITEMS[m_table.cursor()].item) {
                        case Item::PROFILE_PICKER:
                            conductor.push<ProfilePickerStage>();
                            break;
                        case Item::SETTINGS:
                            conductor.push<SettingsStage>();
                            break;
                        }
                    }
                },
                [](const auto&) {},
            };
            std::visit(handler, event);
        }
    };

} // namespace pocketpd
