#pragma once

#include <gmock/gmock.h>

#include <cstdint>

#include "v2/hal/encoder_input.h"

namespace pocketpd {

    class MockEncoderInput : public EncoderInput {
    public:
        MOCK_METHOD(int32_t, position, (), (const, override));
    };

    /**
     * @brief Scripted EncoderInput for tests that just need to set the next
     * position value.
     */
    class FakeEncoderInput : public EncoderInput {
    private:
        int32_t m_position = 0;

    public:
        void set_position(int32_t pos) {
            m_position = pos;
        }

        int32_t position() const override {
            return m_position;
        }
    };

} // namespace pocketpd
