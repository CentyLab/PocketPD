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

    enum class Font : uint8_t {
        SM,
        BASE,
        LG,
        XL,
    };

    class Display {
    public:
        virtual ~Display() = default;

        // —— Buffer lifecycle

        virtual void clear() = 0;
        virtual void flush() = 0;

        // —— Font selection

        virtual void set_font(Font font) = 0;

        // —— Drawing

        /**
         * @brief Draw a bitmap at the given coordinates. Bitmap data is 1 bpp packed 8 pixels per
         * byte horizontally; one row is `width_bytes` bytes. Pixel width therefore equals
         * `width_bytes * 8`. Rows are stored top-to-bottom.
         *
         */
        virtual void draw_bitmap(
            uint8_t x, uint8_t y, uint8_t width_bytes, uint8_t height, const uint8_t* data
        ) = 0;

        /**
         * @brief Draw text at the given coordinates. Text is drawn using the current font.
         *
         */
        virtual void draw_text(uint8_t x, uint8_t y, const char* text) = 0;
        virtual uint16_t text_width(const char* text) = 0;

        virtual void draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) = 0;
    };

} // namespace tempo
