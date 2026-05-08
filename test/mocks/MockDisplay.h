#pragma once

#include <gmock/gmock.h>
#include <tempo/hardware/display.h>

namespace pocketpd {

    class MockDisplay : public tempo::Display {
    public:
        MOCK_METHOD(void, clear, (), (override));
        MOCK_METHOD(void, flush, (), (override));
        MOCK_METHOD(void, set_font, (tempo::Font font), (override));
        MOCK_METHOD(
            void,
            draw_bitmap,
            (uint8_t x, uint8_t y, uint8_t width_bytes, uint8_t height, const uint8_t* data),
            (override)
        );
        MOCK_METHOD(
            void,
            draw_xbm,
            (uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t* data),
            (override)
        );
        MOCK_METHOD(void, draw_text, (uint8_t x, uint8_t y, const char* text), (override));
        MOCK_METHOD(uint16_t, text_width, (const char* text), (override));
        MOCK_METHOD(void, draw_box, (uint8_t x, uint8_t y, uint8_t w, uint8_t h), (override));
    };

} // namespace pocketpd
