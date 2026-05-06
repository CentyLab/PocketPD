/**
 * GoogleTest suite for NormalStage (PDO branch — F5 scope).
 *
 * Drives App::Conductor with MockDisplay, MockPdSink, MockOutputGate and
 * asserts entry RDO call, R-SHORT output toggle, L-LONG → ProfilePicker
 * SELECT, and snapshot-driven render.
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

    EXPECT_CALL(sink, set_pdo(2)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);

    normal.prepare(Profile::PDO, 2);
    conductor.start<NormalStage>();
}

TEST(NormalStage, OnEnterPpsProfileDoesNotIssueRdoInF5) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);

    normal.prepare(Profile::PPS, 1);
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
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(Profile::PDO, 0);
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
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    ProfilePickerStage picker(display, sink);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(picker);
    normal.prepare(Profile::PDO, 0);
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
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(gate, enable).Times(0);
    EXPECT_CALL(gate, disable).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(Profile::PDO, 0);
    conductor.start<NormalStage>();

    normal.on_event(conductor, EncoderEvent{3}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

TEST(NormalStage, OnEnterPdoBranchRendersVAReadoutAndPdoIndex) {
    using ::testing::HasSubstr;
    using ::testing::InSequence;
    using ::testing::StrEq;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(gate, is_enabled()).WillRepeatedly(Return(false));

    InSequence s;
    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(1, 14, StrEq("V"))).Times(1);
    EXPECT_CALL(display, draw_text(::testing::_, 14, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(display, draw_text(1, 47, StrEq("A"))).Times(1);
    EXPECT_CALL(display, draw_text(::testing::_, 47, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(display, draw_text(::testing::_, 50, HasSubstr("[2]"))).Times(::testing::AtLeast(1));
    EXPECT_CALL(display, draw_text(110, 62, StrEq("PDO"))).Times(1);
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, StrEq("OFF"))).Times(::testing::AtLeast(1));
    EXPECT_CALL(display, flush()).Times(1);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(Profile::PDO, 2);
    normal.sensor_snapshot() = SensorSnapshot{0, 5000, 1234, true};

    conductor.start<NormalStage>();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
