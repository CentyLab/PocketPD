/**
 * GoogleTest suite for NormalStage (PDO + PPS branches).
 *
 * Drives App::Conductor with MockDisplay, MockPdSink, MockOutputGate and
 * asserts entry RDO call, R-SHORT output toggle, L-LONG → ProfilePicker
 * SELECT, snapshot-driven render, PPS encoder edits + bounds, increment
 * cycling, and AdjustMode flip.
 */
#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>

#include "v2/app.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/profile_picker_stage.h"

using namespace pocketpd;
using ::testing::NiceMock;
using ::testing::Return;

using TestConductor = App::Conductor;

TEST(NormalStage, OnEnterPdoProfileRequestsFixedPdo) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo(2)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);

    normal.prepare(2);
    conductor.start<NormalStage>();
}

TEST(NormalStage, OnEnterPpsProfileResetsTargetsToDefaults) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo(1, 3300, 1000)).Times(1).WillOnce(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);

    normal.prepare(1);
    conductor.start<NormalStage>();
    EXPECT_EQ(normal.target_mv(), 3300);
    EXPECT_EQ(normal.target_ma(), 1000);
}

TEST(NormalStage, ReEnteringSamePpsProfilePreservesEdits) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(1);
    conductor.start<NormalStage>();

    normal.on_event(conductor, EncoderEvent{-100}, 0);
    ASSERT_EQ(normal.target_mv(), 5300);

    normal.prepare(1);
    normal.on_enter(conductor);
    EXPECT_EQ(normal.target_mv(), 5300);
    EXPECT_EQ(normal.target_ma(), 1000);
}

TEST(NormalStage, SwitchingPpsProfilesResetsTargetsToNewMinimum) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(2)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, pdo_min_voltage_mv(2)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_voltage_mv(2)).WillRepeatedly(Return(20000));
    EXPECT_CALL(sink, pdo_max_current_ma(2)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(1);
    conductor.start<NormalStage>();
    normal.on_event(conductor, EncoderEvent{-100}, 0);
    ASSERT_NE(normal.target_mv(), 3300);

    normal.prepare(2);
    normal.on_enter(conductor);
    EXPECT_EQ(normal.target_mv(), 5000);
    EXPECT_EQ(normal.target_ma(), 1000);
}

TEST(NormalStage, OnEnterWithNegativeIndexRendersNoProfileSelected) {
    using ::testing::HasSubstr;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo).Times(0);
    EXPECT_CALL(display, clear()).Times(::testing::AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, HasSubstr("No Profile Selected")))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(display, flush()).Times(::testing::AtLeast(1));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(-1);
    conductor.start<NormalStage>();
}

TEST(NormalStage, RShortToggleEnablesAndDisablesOutput) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    bool enabled = false;
    EXPECT_CALL(gate, is_enabled()).WillRepeatedly(::testing::ReturnPointee(&enabled));
    EXPECT_CALL(gate, enable()).WillRepeatedly([&] { enabled = true; });
    EXPECT_CALL(gate, disable()).WillRepeatedly([&] { enabled = false; });
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>();

    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_TRUE(enabled);

    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_FALSE(enabled);
}

TEST(NormalStage, LLongRequestsProfilePickerSelect) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    ProfilePickerStage picker(display, sink);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(picker);
    normal.prepare(0);
    conductor.start<NormalStage>();

    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
    EXPECT_EQ(picker.mode(), ProfilePickerStage::Mode::SELECT);
}

TEST(NormalStage, EncoderEventsIgnoredInPdoBranch) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);
    EXPECT_CALL(gate, enable).Times(0);
    EXPECT_CALL(gate, disable).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>();

    normal.on_event(conductor, EncoderEvent{3}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

namespace {

    struct PpsHarness {
        NiceMock<MockDisplay> display;
        NiceMock<MockPdSink> sink;
        NiceMock<MockOutputGate> gate;
        NormalStage normal{display, sink, gate};
        TestConductor conductor;

        explicit PpsHarness(int pdo_index = 1, int v_lo = 3300, int v_hi = 11000, int a_hi = 3000) {
            EXPECT_CALL(sink, is_index_pps(pdo_index)).WillRepeatedly(Return(true));
            EXPECT_CALL(sink, pdo_min_voltage_mv(pdo_index)).WillRepeatedly(Return(v_lo));
            EXPECT_CALL(sink, pdo_max_voltage_mv(pdo_index)).WillRepeatedly(Return(v_hi));
            EXPECT_CALL(sink, pdo_max_current_ma(pdo_index)).WillRepeatedly(Return(a_hi));
            EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));
            conductor.register_stage(normal);
            normal.prepare(static_cast<int8_t>(pdo_index));
            conductor.start<NormalStage>();
        }
    };

} // namespace

TEST(NormalStage, EncoderDeltaInVoltageModeAdjustsTargetMv) {
    PpsHarness h;
    EXPECT_CALL(h.sink, set_pps_pdo(1, 3360, 1000)).Times(1).WillOnce(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-3}, 0);
    EXPECT_EQ(h.normal.target_mv(), 3360);
    EXPECT_EQ(h.normal.adjust_mode(), AdjustMode::VOLTAGE);
}

TEST(NormalStage, EncoderDeltaInCurrentModeAdjustsTargetMa) {
    PpsHarness h;
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.adjust_mode(), AdjustMode::CURRENT);

    EXPECT_CALL(h.sink, set_pps_pdo(1, 3300, 1200)).Times(1).WillOnce(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-4}, 0);
    EXPECT_EQ(h.normal.target_ma(), 1200);
}

TEST(NormalStage, EncoderShortPressCyclesIncrementIndex) {
    PpsHarness h;
    EXPECT_EQ(h.normal.voltage_increment_index(), 0);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.voltage_increment_index(), 1);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.voltage_increment_index(), 2);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.voltage_increment_index(), 0);

    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.adjust_mode(), AdjustMode::CURRENT);
    EXPECT_EQ(h.normal.current_increment_index(), 0);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.current_increment_index(), 1);
    EXPECT_EQ(h.normal.voltage_increment_index(), 0);
}

TEST(NormalStage, EncoderEditUsesActiveIncrementMagnitude) {
    PpsHarness h;
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.voltage_increment_index(), 2);

    EXPECT_CALL(h.sink, set_pps_pdo(1, 4300, 1000)).Times(1).WillOnce(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-1}, 0);
    EXPECT_EQ(h.normal.target_mv(), 4300);
}

TEST(NormalStage, EncoderEditClampsTargetVoltageToPdoBounds) {
    PpsHarness h(1, 3300, 11000, 3000);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);

    EXPECT_CALL(h.sink, set_pps_pdo(1, 11000, ::testing::_)).WillRepeatedly(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-20}, 0);
    EXPECT_EQ(h.normal.target_mv(), 11000);

    h.normal.on_event(h.conductor, EncoderEvent{20}, 0);
    EXPECT_EQ(h.normal.target_mv(), 3300);
}

TEST(NormalStage, EncoderEditClampsCurrentAtZero) {
    PpsHarness h(1, 3300, 11000, 3000);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.adjust_mode(), AdjustMode::CURRENT);

    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-20}, 0);
    ASSERT_EQ(h.normal.target_ma(), 2000);
    h.normal.on_event(h.conductor, EncoderEvent{50}, 0);
    EXPECT_EQ(h.normal.target_ma(), 0);
}

TEST(NormalStage, EncoderEditSnapsTargetVoltageToPpsStepGrid) {
    PpsHarness h(1, 3300, 11000, 3000);
    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-1}, 0);
    EXPECT_EQ(h.normal.target_mv() % PPS_VOLTAGE_STEP_MV, 0);
    h.normal.on_event(h.conductor, EncoderEvent{-1}, 0);
    EXPECT_EQ(h.normal.target_mv() % PPS_VOLTAGE_STEP_MV, 0);
}

TEST(NormalStage, LShortDoesNothingInPdoBranch) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>();

    const auto mode_before = normal.adjust_mode();
    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    EXPECT_EQ(normal.adjust_mode(), mode_before);
}

TEST(NormalStage, OnEnterPdoBranchRendersVAReadoutAndPdoIndex) {
    using ::testing::AtLeast;
    using ::testing::HasSubstr;
    using ::testing::StrEq;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(2)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(2)).WillRepeatedly(Return(20000));
    EXPECT_CALL(sink, pdo_max_current_ma(2)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(gate, is_enabled()).WillRepeatedly(Return(false));

    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber());
    EXPECT_CALL(display, clear()).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(1, 14, StrEq("V"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(1, 47, StrEq("A"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, 27, HasSubstr("20000 mV"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, 62, HasSubstr("5000 mA"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(110, 50, HasSubstr("[2]"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(110, 64, StrEq("PDO"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, StrEq("OFF"))).Times(AtLeast(1));
    EXPECT_CALL(display, flush()).Times(AtLeast(1));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(2);
    normal.on_event(conductor, SensorEvent{SensorSnapshot{0, 5000, 1234, true}}, 0);
    conductor.start<NormalStage>();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
