/**
 * @file display.h
 * @brief Monochrome buffered display interface.
 *
 * Modeled after monochrome OLED panels (1 bit per pixel, byte-aligned rows, baseline-anchored
 * text). See U8g2Display for reference. Drawing operations accumulate into an implementation-owned
 * buffer and are pushed to the panel via flush(). Coordinates are pixel positions, origin top-left.
 */
#pragma once

#include <cstdint>

namespace tempo {

    class Display {
    public:
        virtual ~Display() = default;

        // —— Buffer lifecycle

        virtual void clear() = 0;
        virtual void flush() = 0;

        // —— Drawing
        //
        // Bitmap data is 1 bpp packed 8 pixels per byte horizontally; one row
        // is `width_bytes` bytes. Pixel width therefore equals `width_bytes * 8`. Rows are stored
        // top-to-bottom.

        virtual void draw_bitmap(
            uint8_t x, uint8_t y, uint8_t width_bytes, uint8_t height, const uint8_t* data
        ) = 0;

        // Text Y is the baseline (matches U8g2 / typographic convention).
        virtual void draw_text(uint8_t x, uint8_t y, const char* text) = 0;
        virtual uint16_t text_width(const char* text) = 0;
    };

} // namespace tempo
