/**
 * @file normal_view.h
 * @brief View layer for NormalStage. Renders a NormalViewModel snapshot.
 */
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/hardware/display.h>

#include "v2/events.h"
#include "v2/images.h"
#include "v2/pocketpd.h"
#include "v2/stages/normal/mode.h"

namespace pocketpd {

    struct NormalViewModel {
        int8_t active_pdo_index = -1;
        bool output_enabled = false;
        bool readout_visible = true;
        bool locked = false;
        uint8_t arrow_frame = 0;
        LoadReading load_reading{};
        SupplyReading supply_reading{};
        Mode mode = PassthroughMode{};
        int32_t comp_offset_mv = 0;
    };

    class NormalView {
    private:
        static constexpr uint8_t SCREEN_WIDTH = 128;
        static constexpr uint8_t V_MEASURED_Y = 14;
        static constexpr uint8_t A_MEASURED_Y = 48;
        static constexpr uint8_t V_TARGET_Y = 28;
        static constexpr uint8_t A_TARGET_Y = 62;
        static constexpr uint8_t TARGET_RIGHT_X = 75;
        static constexpr uint8_t STATUS_X = 110;
        static constexpr uint8_t OUTPUT_LABEL_X = 90;

        static constexpr uint8_t ARROW_X = 108;
        static constexpr uint8_t ARROW_Y = 20;
        static constexpr uint8_t ARROW_W = 20;
        static constexpr uint8_t ARROW_H = 20;
        static constexpr std::array<uint8_t, 3> CURSOR_X = {33, 39, 45};
        static constexpr uint8_t CURSOR_W = 6;

    public:
        static constexpr uint8_t PADLOCK_X = 116;
        static constexpr uint8_t PADLOCK_Y = 0;
        static constexpr uint8_t PADLOCK_W = 8;
        static constexpr uint8_t PADLOCK_H = 8;

        static void render(tempo::Display& d, const NormalViewModel& vm) {
            d.clear();

            std::array<char, 32> buf{};

            d.set_font(tempo::Font::XL);
            const uint32_t v_mv = vm.output_enabled ? vm.load_reading.vbus_mv : vm.supply_reading.mv;
            draw_measured(d, "V", V_MEASURED_Y, v_mv, buf, vm.readout_visible);
            draw_measured(
                d, "A", A_MEASURED_Y, vm.load_reading.current_ma, buf, vm.readout_visible
            );

            d.set_font(tempo::Font::BASE);

            std::visit(
                tempo::overloaded{
                    [&](const PassthroughMode&) { draw_passthrough(d); },
                    [&](const FixedMode& mode) { draw_fixed(d, vm, mode, buf); },
                    [&](const PPSMode& mode) { draw_pps(d, vm, mode, buf); },
                },
                vm.mode
            );

            draw_output_indicator(d, vm);

            d.flush();
        }

    private:
        static void draw_measured(
            tempo::Display& d,
            const char* label,
            uint8_t y,
            uint32_t value,
            std::array<char, 32>& buf,
            bool show_value
        ) {
            d.draw_text(1, y, label);
            if (!show_value) {
                return;
            }
            const unsigned long whole = value / 1000;
            const unsigned long fraction = (value % 1000) / 10;
            std::snprintf(buf.data(), buf.size(), "%lu.%02lu", whole, fraction);
            const auto width = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - width), y, buf.data());
        }

        static void draw_passthrough(tempo::Display& d) {
            const char* label = "Passthrough";
            const auto label_w = d.text_width(label);
            const auto label_x = static_cast<uint8_t>((128 - label_w) / 2);
            d.draw_text(label_x, 63, label);
        }

        static void draw_pdo_badge(
            tempo::Display& d, const NormalViewModel& vm, std::array<char, 32>& buf, const char* tag
        ) {
            std::snprintf(buf.data(), buf.size(), "[%u]", vm.active_pdo_index);
            const auto index_x = STATUS_X - d.text_width(buf.data()) - 2;
            d.draw_text(index_x, 63, buf.data());
            d.draw_text(STATUS_X, 64, tag);
        }

        static void draw_fixed(
            tempo::Display& d,
            const NormalViewModel& vm,
            const FixedMode& mode,
            std::array<char, 32>& buf
        ) {
            draw_pdo_badge(d, vm, buf, "PDO");

            std::snprintf(buf.data(), buf.size(), "%ld mV", static_cast<long>(mode.pdo_max_mv));
            auto w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), V_TARGET_Y, buf.data());

            std::snprintf(buf.data(), buf.size(), "%ld mA", static_cast<long>(mode.pdo_max_ma));
            w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), A_TARGET_Y, buf.data());
        }

        static void draw_pps(
            tempo::Display& d,
            const NormalViewModel& vm,
            const PPSMode& mode,
            std::array<char, 32>& buf
        ) {
            draw_pdo_badge(d, vm, buf, "PPS");

            std::snprintf(buf.data(), buf.size(), "%ld mV", static_cast<long>(mode.target_mv));
            auto w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), V_TARGET_Y, buf.data());

            if (vm.comp_offset_mv > 0) {
                std::snprintf(buf.data(), buf.size(), "+%d", vm.comp_offset_mv);
                d.draw_text(TARGET_RIGHT_X + 2, V_TARGET_Y, buf.data());
            }

            std::snprintf(buf.data(), buf.size(), "%ld mA", static_cast<long>(mode.target_ma));
            w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), A_TARGET_Y, buf.data());

            const uint8_t cursor_y =
                ((mode.adjust_mode == AdjustMode::VOLTAGE) ? V_TARGET_Y : A_TARGET_Y) + 1;
            d.draw_box(CURSOR_X.at(mode.cursor_index()), cursor_y, CURSOR_W, 1);
        }

        static void draw_output_indicator(tempo::Display& d, const NormalViewModel& vm) {
            const char* output_state = vm.output_enabled ? "ON" : "OFF";
            const auto output_x = SCREEN_WIDTH - d.text_width(output_state);

            if (vm.locked) {
                d.draw_text(output_x - PADLOCK_W - 12, 8, output_state);
                d.draw_xbm(PADLOCK_X, PADLOCK_Y, PADLOCK_W, PADLOCK_H, bitmap::PADLOCK.data());
            } else {
                d.draw_text(output_x, 8, output_state);
            }

            if (vm.output_enabled) {
                const uint8_t frame = vm.arrow_frame % bitmap::ARROW_FRAMES.size();
                d.draw_xbm(ARROW_X, ARROW_Y, ARROW_W, ARROW_H, bitmap::ARROW_FRAMES.at(frame));
            }
        }
    };

} // namespace pocketpd
