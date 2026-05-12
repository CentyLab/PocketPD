/**
 * @file energy_view.h
 * @brief View layer for the Energy display mode of NormalStage.
 *
 */
#pragma once

#include <tempo/hardware/display.h>

#include <array>
#include <cstdint>
#include <cstdio>

#include "v2/images.h"
#include "v2/state.h"

namespace pocketpd {

    /**
     * @brief Frozen snapshot of everything EnergyView needs to draw one frame.
     */
    struct EnergyViewModel {
        SensorSnapshot snapshot{};
        double accumulated_wh = 0.0;
        double accumulated_ah = 0.0;
        uint32_t total_seconds = 0;
        uint8_t arrow_frame = 0;
        bool output_enabled = false;
    };

    class EnergyView {
    private:
        static constexpr uint8_t ROW1_Y = 8;
        static constexpr uint8_t ROW2_Y = 36;
        static constexpr uint8_t ROW3_Y = 64;

        static constexpr uint8_t COL1_X = 4;
        static constexpr uint8_t COL2_X = 90;

        static constexpr uint8_t V_X = 72;
        static constexpr uint8_t A_X = 72;
        static constexpr uint8_t T_X = 50;

        static constexpr uint8_t ARROW_X = 108;
        static constexpr uint8_t ARROW_Y = 20;
        static constexpr uint8_t ARROW_W = 20;
        static constexpr uint8_t ARROW_H = 20;

        static constexpr uint8_t UNIT_GAP_PX = 2;

        using Display = tempo::Display;

    public:
        static void render(Display& display, const EnergyViewModel& vm) {
            display.clear();
            std::array<char, 16> buf{};

            // The output arrow >>
            if (vm.output_enabled) {
                const auto frame = vm.arrow_frame % bitmap::ARROW_FRAMES.size();
                const auto bitmap = bitmap::ARROW_FRAMES[frame];
                display.draw_xbm(ARROW_X, ARROW_Y, ARROW_W, ARROW_H, bitmap);
            }

            // Top row — live V/A
            display.set_font(tempo::Font::BASE);

            format_milli(buf, vm.snapshot.vbus_mv, 'V');
            display.draw_text(V_X, ROW2_Y - 10, buf.data());

            format_milli(buf, vm.snapshot.current_ma, 'A');
            display.draw_text(A_X, ROW2_Y + 5, buf.data());

            format_time(buf, vm.total_seconds);
            display.draw_text(T_X, ROW3_Y, buf.data());


            format_auto(buf, vm.accumulated_wh);
            draw_value(display, COL1_X, ROW3_Y, buf.data(), "Wh");

            format_auto(buf, vm.accumulated_ah);
            draw_value(display, COL2_X, ROW3_Y, buf.data(), "Ah");

            // Mid — watts and watt-hours
            display.set_font(tempo::Font::XL);
            const double watts = (vm.snapshot.vbus_mv * vm.snapshot.current_ma) / 1'000'000.0;

            format_auto(buf, watts);
            draw_value(display, COL1_X, ROW2_Y, buf.data(), "W");

            display.flush();
        }

    private:
        /**
         * @brief Render a value at (x,y) followed by a unit label 2 px to the right.
         */
        static void
        draw_value(Display& display, uint8_t x, uint8_t y, const char* value, const char* unit) {
            display.draw_text(x, y, value);
            const auto w = display.text_width(value);
            display.draw_text(static_cast<uint8_t>(x + w + UNIT_GAP_PX), y, unit);
        }

        /**
         * @brief Format milli-units (mV / mA) as `X.YY<unit>` packed into one string.
         */
        static void format_milli(std::array<char, 16>& buf, uint32_t milli, char unit) {
            std::snprintf(buf.data(), buf.size(), "%.2f%c", milli / 1000.0, unit);
        }

        /**
         * @brief Auto-pick 2/1/0 decimal places by magnitude.
         */
        static void format_auto(std::array<char, 16>& buf, double value) {
            if (value < 10.0) {
                std::snprintf(buf.data(), buf.size(), "%.2f", value);
            } else if (value < 100.0) {
                std::snprintf(buf.data(), buf.size(), "%.1f", value);
            } else {
                std::snprintf(buf.data(), buf.size(), "%.0f", value);
            }
        }

        /**
         * @brief Format elapsed seconds as `MM:SS` (< 1 h), `HhMMm` (< 1 d), or `DdHHh` (≥ 1 d).
         */
        static void format_time(std::array<char, 16>& buf, uint32_t secs) {
            constexpr uint32_t SEC_PER_MIN = 60;
            constexpr uint32_t SEC_PER_HOUR = 3600;
            constexpr uint32_t SEC_PER_DAY = 86400;

            unsigned long lhs = 0;
            unsigned long rhs = 0;
            const char* fmt = nullptr;

            if (secs < SEC_PER_HOUR) {
                lhs = secs / SEC_PER_MIN;
                rhs = secs % SEC_PER_MIN;
                fmt = "%02lu:%02lu";
            } else if (secs < SEC_PER_DAY) {
                lhs = secs / SEC_PER_HOUR;
                rhs = (secs % SEC_PER_HOUR) / SEC_PER_MIN;
                fmt = "%luh%02lum";
            } else {
                lhs = secs / SEC_PER_DAY;
                rhs = (secs % SEC_PER_DAY) / SEC_PER_HOUR;
                fmt = "%lud%02luh";
            }

            std::snprintf(buf.data(), buf.size(), fmt, lhs, rhs);
        }
    };

} // namespace pocketpd
