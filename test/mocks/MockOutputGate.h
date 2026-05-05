#pragma once

#include <gmock/gmock.h>

#include "v2/hal/output_gate.h"

namespace pocketpd {

    class MockOutputGate : public OutputGate {
    public:
        MOCK_METHOD(void, enable, (), (override));
        MOCK_METHOD(void, disable, (), (override));
        MOCK_METHOD(bool, is_enabled, (), (const, override));
    };

    /**
     * @brief Scripted OutputGate for tests that want to read back the resulting state.
     */
    class FakeOutputGate : public OutputGate {
    private:
        bool m_enabled = false;

    public:
        void enable() override {
            m_enabled = true;
        }

        void disable() override {
            m_enabled = false;
        }

        bool is_enabled() const override {
            return m_enabled;
        }
    };

} // namespace pocketpd
