/**
 * @file menu_stage.h
 * @brief Top-level menu stage.
 *
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/events.h"

namespace pocketpd {

    class MenuStage : public App::Stage, public App::UseLog<MenuStage> {
    private:
        using Display = tempo::Display;

        enum class Item : uint8_t {
            PROFILE_PICKER = 0,
            SETTINGS = 1,
        };

        static constexpr std::array<const char*, 2> ITEM_LABELS = {
            "Profile Picker",
            "Settings",
        };

        Display& m_display;
        uint8_t m_cursor = 0;

        int count() const {
            return ITEM_LABELS.size();
        }

        Item current_item() const {
            return static_cast<Item>(m_cursor);
        }

        void draw() {
            m_display.clear();
            
            for (int i = 0; i < count(); i++) {
                const auto y = (12 * (i + 1));
                if (i == m_cursor) {
                    m_display.draw_text(0, y, ">");
                }

                m_display.draw_text(10, y, ITEM_LABELS[i]);
            }

            m_display.flush();
        }

    public:
        explicit MenuStage(Display& display) : m_display(display) {}

        void on_enter(Conductor&, uint32_t) override {
            m_cursor = 0;
            draw();
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    if (evt.delta == 0) {
                        return;
                    }

                    const int max_idx = count() - 1;
                    int next = std::clamp(m_cursor + evt.delta, 0, max_idx);
                    if (next == m_cursor) {
                        return;
                    }

                    m_cursor = next;
                    draw();
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.request<NormalStage>();
                        return;
                    }
                    if (evt.id == ButtonId::ENCODER && evt.gesture == Gesture::LONG) {
                        if (current_item() == Item::PROFILE_PICKER) {
                            conductor.request<ProfilePickerStage>();
                        } else {
                            conductor.request<SettingsStage>();
                        }
                    }
                },
                [](const auto&) {},
            };
            std::visit(handler, event);
        }
    };

} // namespace pocketpd
