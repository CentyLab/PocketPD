/**
 * @file u8g2_display.h
 * @brief U8g2-backed `tempo::Display` implementation for the SSD1306 128x64 OLED wired over
 * hardware I2C.
 *
 */
#pragma once

#include <tempo/hardware/display.h>

#include "clib/u8g2.h"
#include <U8g2lib.h>

#include "v2/hal/display_orientation.h"

namespace pocketpd {

    class U8g2Display : public tempo::Display, public DisplayOrientation {
    private:
        U8G2_SSD1306_128X64_NONAME_F_HW_I2C m_u8g2{U8G2_R0, U8X8_PIN_NONE};

    public:
        void begin() {
            m_u8g2.begin();
            set_font(tempo::Font::BASE);
        }

        void clear() override {
            m_u8g2.clearBuffer();
        }

        void flush() override {
            m_u8g2.sendBuffer();
        }

        void set_font(tempo::Font font) override {
            switch (font) {
            case tempo::Font::SM:
                m_u8g2.setFont(u8g2_font_profont11_mr);
                break;
            case tempo::Font::BASE:
                m_u8g2.setFont(u8g2_font_profont12_mr);
                break;
            case tempo::Font::LG:
                m_u8g2.setFont(u8g2_font_profont17_mr);
                break;
            case tempo::Font::XL:
                m_u8g2.setFont(u8g2_font_profont22_mr);
                break;
            }
        }

        void draw_bitmap(
            uint8_t x, uint8_t y, uint8_t width_bytes, uint8_t height, const uint8_t* data
        ) override {
            m_u8g2.drawBitmap(x, y, width_bytes, height, data);
        }

        void draw_xbm(
            uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t* data
        ) override {
            m_u8g2.drawXBMP(x, y, width, height, data);
        }

        void draw_text(uint8_t x, uint8_t y, const char* text) override {
            m_u8g2.drawStr(x, y, text);
        }

        uint16_t text_width(const char* text) override {
            return m_u8g2.getStrWidth(text);
        }

        void draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
            m_u8g2.drawBox(x, y, w, h);
        }

        void set_flipped(bool flipped) override {
            m_u8g2.setDisplayRotation(flipped ? U8G2_R2 : U8G2_R0);
        }
    };

} // namespace pocketpd
