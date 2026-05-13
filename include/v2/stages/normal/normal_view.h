/**
 * @file normal_view.h
 * @brief View layer for NormalStage. Renders a NormalViewModel snapshot.
 */
#pragma once

#include <tempo/hardware/display.h>

#include <array>
#include <cstdint>
#include <cstdio>

#include "v2/images.h"
#include "v2/pocketpd.h"
#include "v2/state.h"

namespace pocketpd {

    /**
     * @brief Frozen snapshot of everything NormalView needs to draw one frame.
     *
     */
    struct NormalViewModel {
        int8_t active_pdo_index = -1;
        bool has_profile = false;
        bool is_pps = false;
        bool output_enabled = false;
        bool locked = false;
        uint8_t arrow_frame = 0;
        SensorSnapshot snapshot{};

        // —— PPS branch (valid when has_profile && is_pps)

        int32_t target_mv = 0;
        int32_t target_ma = 0;
        AdjustMode adjust_mode = AdjustMode::VOLTAGE;
        uint8_t cursor_idx = 0;

        // —— Fixed branch (valid when has_profile && !is_pps)

        int32_t pdo_max_mv = 0;
        int32_t pdo_max_ma = 0;
    };

    class NormalView {
    private:
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
        static constexpr uint8_t CURSOR_W = 7;

    public:
        static constexpr uint8_t PADLOCK_X = 116;
        static constexpr uint8_t PADLOCK_Y = 0;
        static constexpr uint8_t PADLOCK_W = 8;
        static constexpr uint8_t PADLOCK_H = 8;

        static void render(tempo::Display& d, const NormalViewModel& vm) {
            d.clear();

            if (!vm.has_profile) {
                d.set_font(tempo::Font::BASE);
                d.draw_text(8, 34, "No Profile Selected");
                d.flush();
                return;
            }

            std::array<char, 32> buf{};

            d.set_font(tempo::Font::XL);
            draw_measured(d, "V", V_MEASURED_Y, vm.snapshot.vbus_mv, buf);
            draw_measured(d, "A", A_MEASURED_Y, vm.snapshot.current_ma, buf);

            d.set_font(tempo::Font::BASE);
            std::snprintf(buf.data(), buf.size(), "[%u]", vm.active_pdo_index);

            const auto INDEX_X = STATUS_X - d.text_width(buf.data()) - 2;
            d.draw_text(INDEX_X, 63, buf.data());
            d.draw_text(STATUS_X, 64, vm.is_pps ? "PPS" : "PDO");

            if (vm.is_pps) {
                draw_target_pps(d, vm, buf);
            } else {
                draw_target_fixed(d, vm, buf);
            }

            if (vm.output_enabled) {
                const uint8_t frame = vm.arrow_frame % bitmap::ARROW_FRAMES.size();
                d.draw_xbm(ARROW_X, ARROW_Y, ARROW_W, ARROW_H, bitmap::ARROW_FRAMES.at(frame));
            }

            if (vm.locked) {
                d.draw_text(INDEX_X + 2, 8, vm.output_enabled ? "ON" : "OFF");
                d.draw_xbm(PADLOCK_X, PADLOCK_Y, PADLOCK_W, PADLOCK_H, bitmap::PADLOCK.data());
            } else {
                d.draw_text(PADLOCK_X - 6, 8, vm.output_enabled ? "ON" : "OFF");
            }

            d.flush();
        }

    private:
        static void draw_measured(
            tempo::Display& d,
            const char* label,
            uint8_t y,
            uint32_t value,
            std::array<char, 32>& buf
        ) {
            d.draw_text(1, y, label);
            const unsigned long whole = value / 1000;
            const unsigned long fraction = (value % 1000) / 10;
            std::snprintf(buf.data(), buf.size(), "%lu.%02lu", whole, fraction);
            const auto width = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - width), y, buf.data());
        }

        static void
        draw_target_fixed(tempo::Display& d, const NormalViewModel& vm, std::array<char, 32>& buf) {
            std::snprintf(buf.data(), buf.size(), "%ld mV", static_cast<long>(vm.pdo_max_mv));
            auto w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), V_TARGET_Y, buf.data());

            std::snprintf(buf.data(), buf.size(), "%ld mA", static_cast<long>(vm.pdo_max_ma));
            w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), A_TARGET_Y, buf.data());
        }

        static void
        draw_target_pps(tempo::Display& d, const NormalViewModel& vm, std::array<char, 32>& buf) {
            std::snprintf(buf.data(), buf.size(), "%ld mV", static_cast<long>(vm.target_mv));
            auto w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), V_TARGET_Y, buf.data());

            std::snprintf(buf.data(), buf.size(), "%ld mA", static_cast<long>(vm.target_ma));
            w = d.text_width(buf.data());
            d.draw_text(static_cast<uint8_t>(TARGET_RIGHT_X - w), A_TARGET_Y, buf.data());

            const uint8_t cursor_y =
                ((vm.adjust_mode == AdjustMode::VOLTAGE) ? V_TARGET_Y : A_TARGET_Y) + 1;
            d.draw_box(CURSOR_X.at(vm.cursor_idx), cursor_y, CURSOR_W, 1);
        }
    };

} // namespace pocketpd
