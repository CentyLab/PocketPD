/**
 * GoogleTest suite for ObtainStage.
 *
 * Drives Conductor<BootStage, ObtainStage> with scripted MockPdSink and a real
 * EventQueue / QueuePublisher so PdReadyEvent payload can be inspected after
 * on_enter runs. Boot→Obtain timeout transition is verified end-to-end here too.
 */
#define VERSION "\"test\""

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <variant>

#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>
#include <tempo/stage/conductor.h>

#include <MockDisplay.h>
#include <MockPdSink.h>

#include "v2/events.h"
#include "v2/stages/boot_stage.h"
#include "v2/stages/obtain_stage.h"

using namespace pocketpd;
using ::testing::NiceMock;
using ::testing::Return;

using TestConductor = tempo::Conductor<BootStage, ObtainStage>;
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

    ObtainStage stage(sink, publisher);
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

    ObtainStage stage(sink, publisher);
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

    ObtainStage stage(sink, publisher);
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

    ObtainStage stage(sink, publisher);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>();
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
    ObtainStage obtain(sink, publisher);
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
