#pragma once

#include <gmock/gmock.h>
#include <tempo/hardware/encoder_input.h>

#include <cstdint>

namespace pocketpd {

    class MockEncoderInput : public tempo::EncoderInput {
    public:
        MOCK_METHOD(int32_t, position, (), (const, override));
    };

    /**
     * @brief Scripted EncoderInput for tests that just need to set the next
     * position value.
     */
    class FakeEncoderInput : public tempo::EncoderInput {
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
