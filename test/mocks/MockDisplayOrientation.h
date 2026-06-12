#pragma once

#include <gmock/gmock.h>

#include "v2/hal/display_orientation.h"

namespace pocketpd {

    class MockDisplayOrientation : public DisplayOrientation {
    public:
        MOCK_METHOD(void, set_flipped, (bool), (override));
    };

    /**
     * @brief Scripted DisplayOrientation for tests that want to read back the resulting state.
     */
    class FakeDisplayOrientation : public DisplayOrientation {
    private:
        bool m_flipped = false;
        int m_call_count = 0;

    public:
        void set_flipped(bool flipped) override {
            m_flipped = flipped;
            ++m_call_count;
        }

        bool flipped() const {
            return m_flipped;
        }

        int call_count() const {
            return m_call_count;
        }
    };

} // namespace pocketpd
