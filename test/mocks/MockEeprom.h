#pragma once

#include <gmock/gmock.h>

#include "v2/hal/eeprom.h"

namespace pocketpd {

    class MockEeprom : public Eeprom {
    public:
        MOCK_METHOD(bool, load, (Preferences & out), (override));
        MOCK_METHOD(bool, save, (const Preferences& in), (override));
    };

} // namespace pocketpd
