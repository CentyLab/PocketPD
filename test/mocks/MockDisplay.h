#pragma once

#include <gmock/gmock.h>
#include <tempo/hardware/display.h>

namespace pocketpd {

    class MockDisplay : public tempo::Display {
    public:
        MOCK_METHOD(void, clear, (), (override));
        MOCK_METHOD(void, flush, (), (override));
        MOCK_METHOD(
            void,
            draw_bitmap,
            (uint8_t x, uint8_t y, uint8_t width_bytes, uint8_t height, const uint8_t* data),
            (override)
        );
        MOCK_METHOD(void, draw_text, (uint8_t x, uint8_t y, const char* text), (override));
        MOCK_METHOD(uint16_t, text_width, (const char* text), (override));
    };

} // namespace pocketpd
