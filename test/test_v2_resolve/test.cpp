#define VERSION "\"test\""

#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "v2/stages/obtain_stage.h"

using namespace pocketpd;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

TEST(ResolveRememberedProfile, RememberOffReturnsFalse) {
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));

    Preferences prefs{.restore_last_profile_enabled = false, .last_profile = {.voltage_mv = 9000}};
    LastProfile out;
    EXPECT_FALSE(resolve_profile(prefs, sink, out));
}

TEST(ResolveRememberedProfile, FixedExactVoltageResolves) {
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, is_index_fixed(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_max_voltage_mv(_)).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(9000));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = false, .voltage_mv = 9000, .pdo_index = 1},
    };
    LastProfile out;
    ASSERT_TRUE(resolve_profile(prefs, sink, out));
    EXPECT_EQ(out.pdo_index, 1);
    EXPECT_EQ(out.voltage_mv, 9000);
    EXPECT_EQ(out.current_ma, 0);
}

TEST(ResolveRememberedProfile, FixedNoMatchingVoltageReturnsFalse) {
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, is_index_fixed(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_max_voltage_mv(_)).WillRepeatedly(Return(20000));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = false, .voltage_mv = 9000},
    };
    LastProfile out;
    EXPECT_FALSE(resolve_profile(prefs, sink, out));
}

TEST(ResolveRememberedProfile, PpsInRangeResolvesAndClampsCurrent) {
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, is_index_pps(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_pps(2)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(2)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(2)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(2)).WillRepeatedly(Return(3000));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = true, .voltage_mv = 9000, .current_ma = 5000, .pdo_index = 2},
    };
    LastProfile out;
    ASSERT_TRUE(resolve_profile(prefs, sink, out));
    EXPECT_EQ(out.pdo_index, 2);
    EXPECT_EQ(out.voltage_mv, 9000);
    EXPECT_EQ(out.current_ma, 3000); // clamped to PDO max
}

TEST(ResolveRememberedProfile, PpsOutOfRangeReturnsFalse) {
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, is_index_pps(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(_)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(_)).WillRepeatedly(Return(11000));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = true, .voltage_mv = 15000, .current_ma = 2000},
    };
    LastProfile out;
    EXPECT_FALSE(resolve_profile(prefs, sink, out));
}

TEST(ResolveRememberedProfile, WrongVoltageAtSavedIndexReturnsFalse) {
    // Same index on a different charger now holds a different fixed voltage.
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(20000));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = false, .voltage_mv = 9000, .pdo_index = 1},
    };
    LastProfile out;
    EXPECT_FALSE(resolve_profile(prefs, sink, out));
    EXPECT_EQ(out.voltage_mv, 0); // out untouched on false
}

TEST(ResolveRememberedProfile, TypeMismatchAtSavedIndexReturnsFalse) {
    // Saved a fixed profile, but that index is now a PPS APDO.
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(false));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = false, .voltage_mv = 9000, .pdo_index = 1},
    };
    LastProfile out;
    EXPECT_FALSE(resolve_profile(prefs, sink, out));
}

TEST(ResolveRememberedProfile, IndexBeyondPdoCountReturnsFalse) {
    // Charger advertises fewer PDOs than when the profile was saved.
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));

    Preferences prefs{
        .restore_last_profile_enabled = true,
        .last_profile = {.is_pps = false, .voltage_mv = 9000, .pdo_index = 5},
    };
    LastProfile out;
    EXPECT_FALSE(resolve_profile(prefs, sink, out));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
