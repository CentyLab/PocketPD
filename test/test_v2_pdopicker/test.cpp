/**
 * @file test.cpp
 * @brief PdoPickerStage render + cursor + transition tests, scripted MockPdSink + MockDisplay.
 */
#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/pocketpd.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/pdo_picker_stage.h"

using namespace pocketpd;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

using TestConductor = App::Conductor;

TEST(PdoPickerStage, ReviewEmptyListRendersFallback) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(8, 34, StrEq("No Profile Detected"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);
    EXPECT_CALL(display, draw_text(::testing::Ne(8), ::testing::_, ::testing::_)).Times(0);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewRendersSingleFixedPdo) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(5, 9, StrEq("PDO: 5V @ 3A"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewRendersFixedAndPpsLines) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));

    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(9000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(2000));

    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(21000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));

    EXPECT_CALL(display, draw_text(5, 9, StrEq("PDO: 9V @ 2A"))).Times(1);
    EXPECT_CALL(display, draw_text(5, 18, StrEq("PPS: 3.3V~21.0V @ 3A"))).Times(1);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewRendersMultiplePpsLines) {
    // issue #24: chargers can advertise multiple PPS APDOs; each must render.
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));

    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(0)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(21000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(5000));

    EXPECT_CALL(display, draw_text(5, 9, StrEq("PPS: 3.3V~11.0V @ 3A"))).Times(1);
    EXPECT_CALL(display, draw_text(5, 18, StrEq("PPS: 3.3V~21.0V @ 5A"))).Times(1);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, SelectEntryDrawsCursorAtFirstRow) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(9000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(2000));

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(0, 9, StrEq(">"))).Times(1);
    EXPECT_CALL(display, draw_text(0, 18, StrEq(">"))).Times(0);
    EXPECT_CALL(display, draw_text(5, 9, StrEq("PDO: 5V @ 3A"))).Times(1);
    EXPECT_CALL(display, draw_text(5, 18, StrEq("PDO: 9V @ 2A"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, SelectEntryDisablesOutput) {
    // issue #32: switching profile must not happen while output is delivering.
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    MockOutputGate output_gate;
    EXPECT_CALL(output_gate, disable()).Times(1);
    EXPECT_CALL(output_gate, enable()).Times(0);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewEntryDoesNotTouchOutput) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    MockOutputGate output_gate;
    EXPECT_CALL(output_gate, disable()).Times(0);
    EXPECT_CALL(output_gate, enable()).Times(0);

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, SelectEncoderMovesCursorAndRerenders) {
    using ::testing::_;
    using ::testing::AnyNumber;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    for (int i = 0; i < 3; ++i) {
        EXPECT_CALL(sink, is_index_fixed(i)).WillRepeatedly(Return(true));
        EXPECT_CALL(sink, is_index_pps(i)).WillRepeatedly(Return(false));
        EXPECT_CALL(sink, pdo_max_voltage_mv(i)).WillRepeatedly(Return(5000));
        EXPECT_CALL(sink, pdo_max_current_ma(i)).WillRepeatedly(Return(3000));
    }

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();

    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(5, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(0, 9, StrEq(">"))).Times(0);
    EXPECT_CALL(display, draw_text(0, 18, StrEq(">"))).Times(1);
    EXPECT_CALL(display, draw_text(0, 27, StrEq(">"))).Times(0);
    EXPECT_CALL(display, flush()).Times(1);

    stage.on_event(conductor, EncoderEvent{1}, 0);
}

TEST(PdoPickerStage, SelectEncoderClampsAtTopAndBottom) {
    using ::testing::_;
    using ::testing::AnyNumber;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    for (int i = 0; i < 2; ++i) {
        EXPECT_CALL(sink, is_index_fixed(i)).WillRepeatedly(Return(true));
        EXPECT_CALL(sink, is_index_pps(i)).WillRepeatedly(Return(false));
        EXPECT_CALL(sink, pdo_max_voltage_mv(i)).WillRepeatedly(Return(5000));
        EXPECT_CALL(sink, pdo_max_current_ma(i)).WillRepeatedly(Return(3000));
    }

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(0);
    EXPECT_CALL(display, flush()).Times(0);
    stage.on_event(conductor, EncoderEvent{-5}, 0);
    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(5, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(0, 9, StrEq(">"))).Times(0);
    EXPECT_CALL(display, draw_text(0, 18, StrEq(">"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);
    stage.on_event(conductor, EncoderEvent{10}, 0);
    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(0);
    EXPECT_CALL(display, flush()).Times(0);
    stage.on_event(conductor, EncoderEvent{4}, 0);
}

TEST(PdoPickerStage, SelectEmptyListIgnoresEncoder) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(0);
    EXPECT_CALL(display, flush()).Times(0);
    stage.on_event(conductor, EncoderEvent{1}, 0);
}

TEST(PdoPickerStage, ReviewEncoderRequestsSelectModeReentry) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(9000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(2000));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();

    stage.on_event(conductor, EncoderEvent{1}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<PdoPickerStage>());
    EXPECT_EQ(stage.mode(), PdoPickerStage::Mode::SELECT);
    EXPECT_FALSE(output_gate.is_enabled());
}

TEST(PdoPickerStage, ReviewShortButtonRequestsNormalPpsWhenChargerHasPps) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(1));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::OUTPUT_TOGGLE, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PPS);
}

TEST(PdoPickerStage, ReviewShortButtonRequestsNormalPdoWhenNoPps) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::OUTPUT_TOGGLE, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
}

TEST(PdoPickerStage, ReviewTimeoutTransitionsToNormal) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    conductor.tick(0);
    conductor.tick(PDOPICKER_REVIEW_TO_NORMAL_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(PDOPICKER_REVIEW_TO_NORMAL_MS);
    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
}

TEST(PdoPickerStage, SelectLongPressCommitsPpsWhenCursorOnPps) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(21000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    stage.on_event(conductor, EncoderEvent{1}, 0);
    stage.on_event(conductor, ButtonEvent{ButtonId::SELECT_VI, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PPS);
}

TEST(PdoPickerStage, SelectLongPressCommitsPdoWhenCursorOnFixed) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    // Cursor stays at row 0 (fixed) — no encoder rotation.
    stage.on_event(conductor, ButtonEvent{ButtonId::SELECT_VI, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
}

TEST(PdoPickerStage, SelectLongPressIgnoredWhenEmptyPdoList) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::SELECT_VI, Gesture::LONG}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

TEST(PdoPickerStage, SelectIgnoresLongPressOnNonSelectVi) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    NormalStage normal;
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<PdoPickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    stage.on_event(conductor, ButtonEvent{ButtonId::OUTPUT_TOGGLE, Gesture::LONG}, 0);
    EXPECT_FALSE(conductor.has_pending());

    stage.on_event(conductor, ButtonEvent{ButtonId::SELECT_VI, Gesture::SHORT}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

TEST(PdoPickerStage, ReviewClearsBeforeDrawingAndFlushesAfter) {
    NiceMock<MockPdSink> sink;
    FakeOutputGate output_gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    MockDisplay display;
    {
        InSequence seq;
        EXPECT_CALL(display, clear());
        EXPECT_CALL(display, draw_text(5, 9, StrEq("PDO: 5V @ 3A")));
        EXPECT_CALL(display, flush());
    }

    PdoPickerStage stage(display, sink, output_gate);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
