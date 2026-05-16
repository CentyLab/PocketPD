/**
 * GoogleTest suite for SensorTask.
 *
 * Drives a FakePowerMonitor through scripted readings; asserts SensorTask
 * publishes a SensorEvent carrying the snapshot with the tick timestamp.
 */
#define VERSION "\"test\""

#include <MockPowerMonitor.h>
#include <MockSupplyVoltageSource.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>

#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/tasks/sensor_task.h"

using namespace pocketpd;

using TestQueue = tempo::EventQueue<Event, 8>;
using TestPublisher = tempo::QueuePublisher<Event, 8>;

namespace {

    const SensorEvent* pop_sensor(TestQueue& q) {
        static Event last;
        if (!q.pop(last)) {
            return nullptr;
        }
        return std::get_if<SensorEvent>(&last);
    }

} // namespace

TEST(SensorTask, OnTickPublishesFusedLoadAndSupply) {
    FakePowerMonitor monitor;
    FakeSupplyVoltageSource supply;
    TestQueue q;
    TestPublisher pub(q);
    SensorTask task(monitor, supply);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    monitor.push({5000, 1234, true});
    supply.push({20000, true});
    task.on_tick(42);

    const auto* evt = pop_sensor(q);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->load.timestamp_ms, 42u);
    EXPECT_EQ(evt->load.vbus_mv, 5000u);
    EXPECT_EQ(evt->load.current_ma, 1234u);
    EXPECT_EQ(evt->supply.timestamp_ms, 42u);
    EXPECT_EQ(evt->supply.mv, 20000u);
    EXPECT_TRUE(evt->supply.valid);
}

TEST(SensorTask, BothInvalidDoesNotPublish) {
    FakePowerMonitor monitor;
    FakeSupplyVoltageSource supply;
    TestQueue q;
    TestPublisher pub(q);
    SensorTask task(monitor, supply);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    monitor.push({0, 0, false});
    supply.push({0, false});
    task.on_tick(99);

    EXPECT_EQ(pop_sensor(q), nullptr);
}

TEST(SensorTask, LoadValidSupplyInvalidStillPublishesWithInvalidSupply) {
    FakePowerMonitor monitor;
    FakeSupplyVoltageSource supply;
    TestQueue q;
    TestPublisher pub(q);
    SensorTask task(monitor, supply);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    monitor.push({3000, 500, true});
    supply.push({0, false});
    task.on_tick(100);

    const auto* evt = pop_sensor(q);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->load.vbus_mv, 3000u);
    EXPECT_FALSE(evt->supply.valid);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
