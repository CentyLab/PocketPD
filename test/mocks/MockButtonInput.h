#pragma once

#include <gmock/gmock.h>
#include <tempo/hardware/button_input.h>

namespace pocketpd {

    class MockButtonInput : public tempo::ButtonInput {
    public:
        MOCK_METHOD(void, update, (), (override));
        MOCK_METHOD(bool, is_held, (), (const, override));
    };

    /**
     * @brief Scripted ButtonInput for tests that just need to flip held state.
     */
    class FakeButtonInput : public tempo::ButtonInput {
    private:
        bool m_held = false;

    public:
        void set_held(bool held) {
            m_held = held;
        }

        void update() override {}

        bool is_held() const override {
            return m_held;
        }
    };

} // namespace pocketpd
