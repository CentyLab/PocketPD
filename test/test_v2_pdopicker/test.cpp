/**
 * GoogleTest suite for PdoPickerStage REVIEW render.
 *
 * Drives the conductor with scripted MockPdSink + MockDisplay, asserts
 * the draw_text call sequence matches the v1 PDO list layout.
 */
#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>

#include "v2/app.h"
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
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(8, 34, StrEq("No Profile Detected"))).Times(1);
    EXPECT_CALL(display, flush()).Times(1);
    EXPECT_CALL(display, draw_text(::testing::Ne(8), ::testing::_, ::testing::_)).Times(0);

    PdoPickerStage stage(display, sink);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewRendersSingleFixedPdo) {
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

    PdoPickerStage stage(display, sink);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewRendersFixedAndPpsLines) {
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

    PdoPickerStage stage(display, sink);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewRendersMultiplePpsLines) {
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

    PdoPickerStage stage(display, sink);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, SelectEntryDoesNotDraw) {
    // SELECT mode: render + cursor + output-disable land in F4. Today: log-only.
    using ::testing::_;

    MockDisplay display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(display, clear()).Times(0);
    EXPECT_CALL(display, draw_text(_, _, _)).Times(0);
    EXPECT_CALL(display, flush()).Times(0);

    PdoPickerStage stage(display, sink);
    stage.prepare(PdoPickerStage::Mode::SELECT);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

TEST(PdoPickerStage, ReviewClearsBeforeDrawingAndFlushesAfter) {
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

    PdoPickerStage stage(display, sink);
    stage.prepare(PdoPickerStage::Mode::REVIEW);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<PdoPickerStage>();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
