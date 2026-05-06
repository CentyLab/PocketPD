#pragma once

#include <gmock/gmock.h>

#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class MockPdSink : public PdSinkController {
    public:
        MOCK_METHOD(bool, begin, (), (override));
        MOCK_METHOD(int, pdo_count, (), (const, override));
        MOCK_METHOD(int, pps_count, (), (const, override));
        MOCK_METHOD(bool, is_index_pps, (int index), (const, override));
        MOCK_METHOD(bool, is_index_fixed, (int index), (const, override));
        MOCK_METHOD(int, pdo_max_voltage_mv, (int index), (const, override));
        MOCK_METHOD(int, pdo_min_voltage_mv, (int index), (const, override));
        MOCK_METHOD(int, pdo_max_current_ma, (int index), (const, override));
        MOCK_METHOD(bool, set_pdo, (int index), (override));
        MOCK_METHOD(bool, set_pps_pdo, (int index, int voltage_mv, int current_ma), (override));
        MOCK_METHOD(int, default_index_for, (Profile profile), (const, override));
    };

} // namespace pocketpd
