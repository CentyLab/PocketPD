#define VERSION "\"test\""

#include <MockEeprom.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/preferences_store.h"
#include "v2/tasks/pps_control_task.h"

using namespace pocketpd;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace {

    struct Harness {
        NiceMock<MockPdSink> sink;
        NiceMock<MockOutputGate> gate;
        NiceMock<MockEeprom> eeprom;
        PreferencesStore prefs{eeprom};
        PpsControlTask task{prefs, gate, sink};

        Harness() {
            ON_CALL(sink, pdo_max_voltage_mv(_)).WillByDefault(Return(11000));
        }
    };

    LoadReading load_with(uint32_t mv, uint32_t ma, uint32_t ts = 1) {
        return LoadReading{ts, mv, ma};
    }

} // namespace

// —— Sentinel + ignore paths

TEST(PpsControlTask, SentinelPpsTargetMakesNoSinkCall) {
    Harness h;
    EXPECT_CALL(h.sink, set_pps_pdo).Times(0);

    h.task.on_event(PpsTargetEvent{-1, 0, 0}, 0);
}

TEST(PpsControlTask, IgnoresUnrelatedEventTypes) {
    Harness h;
    EXPECT_CALL(h.sink, set_pps_pdo).Times(0);

    h.task.on_event(ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    h.task.on_event(EncoderEvent{1}, 0);
    h.task.on_event(EnergyEvent{1.0, 1.0, 1}, 0);
    h.task.on_event(CompStateEvent{42}, 0);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);
}

// —— Write-on-PpsTargetEvent (task is sole sink writer)

TEST(PpsControlTask, WritesBareTargetOnPpsTargetWhenDisabled) {
    Harness h;
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).Times(1).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
}

TEST(PpsControlTask, WritesBareTargetOnPpsTargetWhenEnabledButOffsetIsZero) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).Times(1).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
}

TEST(PpsControlTask, ReissuesWithOffsetOnSamePdoTargetChange) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));

    // Build offset = 60 via ticks.
    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    for (int i = 0; i < 3; ++i) {
        h.task.on_tick((i + 1) * PpsControlTask::PERIOD_MS);
    }
    EXPECT_EQ(h.task.comp_offset_mv(), 60);

    // User turns encoder; NormalStage publishes new target. Task must re-issue with offset.
    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5160, 1000)).Times(1).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5100, 1000}, 1000);
}

TEST(PpsControlTask, NewPdoResetsOffsetAndWritesBareTarget) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));

    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 20);

    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo(1, 9000, 2000)).Times(1).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{1, 9000, 2000}, 500);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);
}

// —— Closed-loop control

TEST(PpsControlTaskTick, StepsUpOncePerTickWhenErrorPositive) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));

    ::testing::InSequence seq;
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).WillOnce(Return(true));
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5020, 1000)).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 20);
}

TEST(PpsControlTaskTick, DeadBandSuppressesUpdates) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).Times(1).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4985, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);
}

TEST(PpsControlTaskTick, NegativeErrorStepsDown) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));

    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    for (int i = 0; i < 5; ++i) {
        h.task.on_tick((i + 1) * PpsControlTask::PERIOD_MS);
    }
    EXPECT_EQ(h.task.comp_offset_mv(), 100);

    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5080, 1000)).WillOnce(Return(true));
    h.task.on_event(SensorEvent{load_with(5050, 1500), {}}, 2000);
    h.task.on_tick(2000);
    EXPECT_EQ(h.task.comp_offset_mv(), 80);
}

TEST(PpsControlTaskTick, OutputOffResetsOffset) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);

    EXPECT_CALL(h.gate, is_enabled())
        .WillOnce(Return(true))
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(h.sink, set_pps_pdo).Times(3).WillRepeatedly(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    h.task.on_tick(2 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 40);

    // Output off: the learned offset is stale, so drop it and rewrite the bare target.
    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).Times(1).WillOnce(Return(true));
    h.task.on_tick(3 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);

    // Already zeroed — no further writes while output stays off.
    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo).Times(0);
    h.task.on_tick(4 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);
}

TEST(PpsControlTaskTick, SaturatesAtMaxComp) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4000, 1500), {}}, 1);

    for (int i = 0; i < 30; ++i) {
        h.task.on_tick((i + 1) * PpsControlTask::PERIOD_MS);
    }
    EXPECT_EQ(h.task.comp_offset_mv(), 500);

    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo).Times(0);
    h.task.on_tick(31 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 500);
}

TEST(PpsControlTaskTick, FailedSinkOnTickHoldsOffsetForRetry) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));

    {
        ::testing::InSequence seq;
        EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).WillOnce(Return(true));   // initial OK
        EXPECT_CALL(h.sink, set_pps_pdo(0, 5020, 1000)).WillOnce(Return(false));  // tick fails
        EXPECT_CALL(h.sink, set_pps_pdo(0, 5020, 1000)).WillOnce(Return(true));   // retry OK
    }

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);

    h.task.on_tick(2 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 20);
}

TEST(PpsControlTaskTick, PreferenceFlipOffEmitsCleanupAndZeroes) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));

    EXPECT_CALL(h.sink, set_pps_pdo).Times(3).WillRepeatedly(Return(true));
    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    h.task.on_tick(2 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 40);

    h.prefs.set_voltage_comp_enabled(false);
    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).Times(1).WillOnce(Return(true));

    h.task.on_tick(3 * PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);

    ::testing::Mock::VerifyAndClearExpectations(&h.sink);
    EXPECT_CALL(h.sink, set_pps_pdo).Times(0);
    h.task.on_tick(4 * PpsControlTask::PERIOD_MS);
}

TEST(PpsControlTaskTick, ClampsRequestToPdoMax) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(h.sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));

    // target == pdo_max: initial write at 5000 happens; on_tick saturates at offset=0.
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).Times(1).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 0);
}

TEST(PpsControlTaskTick, PartialPdoClampStopsAtCeiling) {
    Harness h;
    h.prefs.set_voltage_comp_enabled(true);
    EXPECT_CALL(h.gate, is_enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(h.sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5010));

    ::testing::InSequence seq;
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5000, 1000)).WillOnce(Return(true));
    EXPECT_CALL(h.sink, set_pps_pdo(0, 5010, 1000)).WillOnce(Return(true));

    h.task.on_event(PpsTargetEvent{0, 5000, 1000}, 0);
    h.task.on_event(SensorEvent{load_with(4800, 1500), {}}, 1);
    h.task.on_tick(PpsControlTask::PERIOD_MS);
    EXPECT_EQ(h.task.comp_offset_mv(), 10);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
