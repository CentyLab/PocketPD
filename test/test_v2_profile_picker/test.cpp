/**
 * @file test.cpp
 * @brief ProfilePickerStage render + cursor + transition tests, scripted MockPdSink + MockDisplay.
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
#include "v2/stages/profile_picker_stage.h"

using namespace pocketpd;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

using TestConductor = App::Conductor;

TEST(ProfilePickerStage, ReviewEmptyListRendersFallback) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(8, 34, StrEq("No Profile Detected"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);
    EXPECT_CALL(display, draw_text(::testing::Ne(8), ::testing::_, ::testing::_)).Times(0);

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
}

TEST(ProfilePickerStage, ReviewRendersSingleFixedPdo) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(5, 9, StrEq("PDO: 5V @ 3A"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
}

TEST(ProfilePickerStage, ReviewRendersFixedAndPpsLines) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
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

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
}

TEST(ProfilePickerStage, ReviewRendersMultiplePpsLines) {
    // issue #24: chargers can advertise multiple PPS APDOs; each must render.
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
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

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
}

TEST(ProfilePickerStage, SelectEntryDrawsCursorAtFirstRow) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
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

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
}

TEST(ProfilePickerStage, SelectEncoderMovesCursorAndRerenders) {
    using ::testing::_;
    using ::testing::AnyNumber;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    for (int i = 0; i < 3; ++i) {
        EXPECT_CALL(sink, is_index_fixed(i)).WillRepeatedly(Return(true));
        EXPECT_CALL(sink, is_index_pps(i)).WillRepeatedly(Return(false));
        EXPECT_CALL(sink, pdo_max_voltage_mv(i)).WillRepeatedly(Return(5000));
        EXPECT_CALL(sink, pdo_max_current_ma(i)).WillRepeatedly(Return(3000));
    }

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();

    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(5, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(0, 9, StrEq(">"))).Times(0);
    EXPECT_CALL(display, draw_text(0, 18, StrEq(">"))).Times(1);
    EXPECT_CALL(display, draw_text(0, 27, StrEq(">"))).Times(0);
    EXPECT_CALL(display, flush()).Times(1);

    stage.on_event(conductor, EncoderEvent{1}, 0);
}

TEST(ProfilePickerStage, SelectEncoderClampsAtTopAndBottom) {
    using ::testing::_;
    using ::testing::AnyNumber;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    for (int i = 0; i < 2; ++i) {
        EXPECT_CALL(sink, is_index_fixed(i)).WillRepeatedly(Return(true));
        EXPECT_CALL(sink, is_index_pps(i)).WillRepeatedly(Return(false));
        EXPECT_CALL(sink, pdo_max_voltage_mv(i)).WillRepeatedly(Return(5000));
        EXPECT_CALL(sink, pdo_max_current_ma(i)).WillRepeatedly(Return(3000));
    }

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
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

TEST(ProfilePickerStage, SelectEmptyListIgnoresEncoder) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
    ::testing::Mock::VerifyAndClearExpectations(&display);

    EXPECT_CALL(display, clear()).Times(0);
    EXPECT_CALL(display, flush()).Times(0);
    stage.on_event(conductor, EncoderEvent{1}, 0);
}

TEST(ProfilePickerStage, ReviewEncoderRequestsSelectModeReentry) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, is_index_fixed(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(9000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(2000));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, EncoderEvent{1}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
    EXPECT_EQ(stage.mode(), ProfilePickerStage::Mode::SELECT);
}

TEST(ProfilePickerStage, ReviewShortButtonRequestsNormalPpsWhenChargerHasPps) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, default_index_for(Profile::PPS)).WillRepeatedly(Return(1));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PPS);
    EXPECT_EQ(normal.active_pdo_index(), 1);
}

TEST(ProfilePickerStage, ReviewShortButtonRequestsNormalPdoWhenNoPps) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, default_index_for(Profile::PDO)).WillRepeatedly(Return(0));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
    EXPECT_EQ(normal.active_pdo_index(), 0);
}

TEST(ProfilePickerStage, ReviewTimeoutTransitionsToNormal) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, default_index_for(Profile::PDO)).WillRepeatedly(Return(0));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    conductor.tick(0);
    conductor.tick(PROFILE_PICKER_REVIEW_TO_NORMAL_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(PROFILE_PICKER_REVIEW_TO_NORMAL_MS);
    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
    EXPECT_EQ(normal.active_pdo_index(), 0);
}

TEST(ProfilePickerStage, SelectLongPressCommitsPpsWhenCursorOnPps) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
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

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, EncoderEvent{1}, 0);
    stage.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PPS);
    EXPECT_EQ(normal.active_pdo_index(), 1);
}

TEST(ProfilePickerStage, SelectLongPressCommitsPdoWhenCursorOnFixed) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    // Cursor stays at row 0 (fixed) — no encoder rotation.
    stage.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
    EXPECT_EQ(normal.active_pdo_index(), 0);
}

TEST(ProfilePickerStage, SelectLongPressIgnoredWhenEmptyPdoList) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

TEST(ProfilePickerStage, SelectIgnoresRLongPressAndEncoderShortPress) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);
    stage.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

TEST(ProfilePickerStage, SelectLongPressLExitsToNormalWithoutChangingProfile) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, is_index_fixed(0)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(0)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_current_ma(0)).WillRepeatedly(Return(3000));

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    normal.prepare(Profile::PPS); // simulate prior session
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PPS); // unchanged
}

TEST(ProfilePickerStage, ReviewClearsBeforeDrawingAndFlushesAfter) {
    NiceMock<MockPdSink> sink;
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

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ProfilePickerStage>();
}

TEST(ProfilePickerStage, SelectCommitPromotesPendingCursorAndPreservesAcrossReentry) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    for (int i = 0; i < 3; ++i) {
        EXPECT_CALL(sink, is_index_fixed(i)).WillRepeatedly(Return(true));
        EXPECT_CALL(sink, is_index_pps(i)).WillRepeatedly(Return(false));
        EXPECT_CALL(sink, pdo_max_voltage_mv(i)).WillRepeatedly(Return(5000));
        EXPECT_CALL(sink, pdo_max_current_ma(i)).WillRepeatedly(Return(3000));
    }

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    // Move cursor and commit — committed cursor follows pending only on commit.
    stage.on_event(conductor, EncoderEvent{2}, 0);
    EXPECT_EQ(stage.cursor_index(), 0); // committed unchanged before commit
    stage.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    EXPECT_EQ(stage.cursor_index(), 2);

    // Re-enter SELECT — committed cursor must persist.
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    stage.on_enter(conductor);
    EXPECT_EQ(stage.cursor_index(), 2);
}

TEST(ProfilePickerStage, SelectExitViaLDoesNotPromotePendingCursor) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    for (int i = 0; i < 3; ++i) {
        EXPECT_CALL(sink, is_index_fixed(i)).WillRepeatedly(Return(true));
        EXPECT_CALL(sink, is_index_pps(i)).WillRepeatedly(Return(false));
        EXPECT_CALL(sink, pdo_max_voltage_mv(i)).WillRepeatedly(Return(5000));
        EXPECT_CALL(sink, pdo_max_current_ma(i)).WillRepeatedly(Return(3000));
    }

    ProfilePickerStage stage(display, sink);
    stage.prepare(ProfilePickerStage::Mode::SELECT);
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ProfilePickerStage>();

    stage.on_event(conductor, EncoderEvent{2}, 0);
    stage.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);

    EXPECT_EQ(stage.cursor_index(), 0); // committed cursor untouched by L exit
}

TEST(NormalStage, LongPressLReturnsToProfilePicker) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    ProfilePickerStage picker(display, sink);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(picker);
    normal.prepare(Profile::PDO);
    conductor.start<NormalStage>();

    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
    EXPECT_EQ(picker.mode(), ProfilePickerStage::Mode::SELECT);
}

TEST(NormalStage, IgnoresOtherButtonsAndShortPressOnL) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    ProfilePickerStage picker(display, sink);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(picker);
    normal.prepare(Profile::PDO);
    conductor.start<NormalStage>();

    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);
    normal.on_event(conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_FALSE(conductor.has_pending());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
