/**
 * GoogleTest suite for ObtainStage.
 *
 * Drives Conductor<...> with scripted MockPdSink and a real EventQueue /
 * QueuePublisher so PdReadyEvent payload can be inspected. Also covers the
 * input-driven exits (short button → resume, encoder → ProfilePicker SELECT,
 * timeout → ProfilePicker REVIEW).
 */
#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>
#include <tempo/stage/conductor.h>

#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/stages/boot_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/obtain_stage.h"
#include "v2/stages/profile_picker_stage.h"

using namespace pocketpd;
using ::testing::NiceMock;
using ::testing::Return;

using TestConductor = App::Conductor;
using TestQueue = tempo::EventQueue<Event, 8>;
using TestPublisher = tempo::QueuePublisher<Event, 8>;

namespace {

    PdReadyEvent expect_pd_ready(TestQueue& q) {
        Event e;
        EXPECT_TRUE(q.pop(e));
        const auto* ready = std::get_if<PdReadyEvent>(&e);
        EXPECT_NE(ready, nullptr);
        return ready ? *ready : PdReadyEvent{};
    }

} // namespace

TEST(ObtainStage, OnEnterCallsBeginAndPublishesCount) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(3));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(1));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>();

    const auto ready = expect_pd_ready(queue);
    EXPECT_EQ(ready.num_pdo, 3);
    EXPECT_EQ(ready.pps_count, 1);
}

TEST(ObtainStage, MultiPpsChargerReportsBothPps) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(5));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(2));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>();

    const auto ready = expect_pd_ready(queue);
    EXPECT_EQ(ready.num_pdo, 5);
    EXPECT_EQ(ready.pps_count, 2);
}

TEST(ObtainStage, BeginFailureSuppressesPdReady) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);

    EXPECT_CALL(sink, begin()).WillOnce(Return(false));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>();

    Event e;
    EXPECT_FALSE(queue.pop(e));
}

TEST(ObtainStage, OnEnterIssuesNoRdoRequest) {
    // Issue #33: charger-dependent default voltage. Obtain must not pick a PDO.
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>();
}

TEST(ObtainStage, ShortButtonResumesNormalInPpsProfileAfterPdReady) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(1));
    EXPECT_CALL(sink, default_index_for(Profile::PPS)).WillRepeatedly(Return(2));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    NiceMock<MockDisplay> normal_display;
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(normal_display, sink, normal_gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ObtainStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PPS);
    EXPECT_EQ(normal.active_pdo_index(), 2u);
}

TEST(ObtainStage, ShortButtonResumesNormalInPdoProfileWhenNoPps) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, default_index_for(Profile::PDO)).WillRepeatedly(Return(0));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    NiceMock<MockDisplay> normal_display;
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(normal_display, sink, normal_gate);
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ObtainStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.profile(), Profile::PDO);
    EXPECT_EQ(normal.active_pdo_index(), 0u);
}

TEST(ObtainStage, ShortButtonIgnoredWhenPdNotReady) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);
    EXPECT_CALL(sink, begin()).WillOnce(Return(false));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>();

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

TEST(ObtainStage, EncoderRotationJumpsToProfilePickerInSelectMode) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    NiceMock<MockDisplay> picker_display;
    ProfilePickerStage picker(picker_display, sink);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(picker);
    conductor.start<ObtainStage>();

    stage.on_event(conductor, EncoderEvent{-1}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
    EXPECT_EQ(picker.mode(), ProfilePickerStage::Mode::SELECT);
}

TEST(ObtainStage, TimeoutTransitionsToProfilePickerInReviewMode) {
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));

    ObtainStage stage(sink);
    stage.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    NiceMock<MockDisplay> picker_display;
    ProfilePickerStage picker(picker_display, sink);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(picker);
    conductor.start<ObtainStage>();

    conductor.tick(0);
    conductor.tick(OBTAIN_TO_PROFILE_PICKER_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(OBTAIN_TO_PROFILE_PICKER_MS);
    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
    EXPECT_EQ(picker.mode(), ProfilePickerStage::Mode::REVIEW);
}

TEST(BootStage, RequestsObtainAfterTimeout) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    TestQueue queue;
    TestPublisher publisher(queue);

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, pps_count()).WillRepeatedly(Return(0));

    BootStage boot(display);
    ObtainStage obtain(sink);
    obtain.attach_publisher_INTERNAL_DO_NOT_USE(publisher);
    TestConductor conductor;
    conductor.register_stage(boot);
    conductor.register_stage(obtain);
    conductor.start<BootStage>();

    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<BootStage>());

    conductor.tick(0);
    conductor.tick(BOOT_TO_OBTAIN_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(BOOT_TO_OBTAIN_MS);
    EXPECT_TRUE(conductor.has_pending());

    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ObtainStage>());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
