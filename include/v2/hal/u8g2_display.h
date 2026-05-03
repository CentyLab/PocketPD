/**
 * @file u8g2_display.h
 * @brief U8g2-backed `tempo::Display` implementation for the SSD1306 128x64 OLED wired over
 * hardware I2C.
 *
 */
#pragma once

#ifdef ARDUINO

#include <tempo/hardware/display.h>

#include <U8g2lib.h>

namespace pocketpd {

    class U8g2Display : public tempo::Display {
    private:
        U8G2_SSD1306_128X64_NONAME_F_HW_I2C m_u8g2{U8G2_R0, U8X8_PIN_NONE};

    public:
        void begin() {
            m_u8g2.begin();
            m_u8g2.setFont(u8g2_font_profont11_tr);
        }

        void clear() override {
            m_u8g2.clearBuffer();
        }

        void flush() override {
            m_u8g2.sendBuffer();
        }

        void draw_bitmap(
            uint8_t x, uint8_t y, uint8_t width_bytes, uint8_t height, const uint8_t* data
        ) override {
            m_u8g2.drawBitmap(x, y, width_bytes, height, data);
        }

        void draw_text(uint8_t x, uint8_t y, const char* text) override {
            m_u8g2.drawStr(x, y, text);
        }

        uint16_t text_width(const char* text) override {
            return m_u8g2.getStrWidth(text);
        }
    };

} // namespace pocketpd

#endif // ARDUINO
