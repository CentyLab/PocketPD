/**
 * GoogleTest suite for SensorTask.
 *
 * Drives a FakePowerMonitor through scripted readings; asserts SensorTask
 * copies them into NormalStage::sensor_snapshot() with the tick timestamp.
 */
#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <MockPowerMonitor.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "v2/stages/normal_stage.h"
#include "v2/tasks/sensor_task.h"

using namespace pocketpd;
using ::testing::NiceMock;

TEST(SensorTask, PollWritesSnapshotWithTimestampAndValues) {
    FakePowerMonitor monitor;
    NiceMock<MockDisplay> normal_display;
    NiceMock<MockPdSink> normal_sink;
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(normal_display, normal_sink, normal_gate);

    monitor.push({5000, 1234, true});

    SensorTask task(monitor, normal);
    task.poll(42);

    const auto& snap = normal.sensor_snapshot();
    EXPECT_EQ(snap.timestamp_ms, 42u);
    EXPECT_EQ(snap.vbus_mv, 5000u);
    EXPECT_EQ(snap.current_ma, 1234u);
    EXPECT_TRUE(snap.valid);
}

TEST(SensorTask, InvalidReadingPropagatesValidFalse) {
    FakePowerMonitor monitor;
    NiceMock<MockDisplay> normal_display;
    NiceMock<MockPdSink> normal_sink;
    NiceMock<MockOutputGate> normal_gate;
    NormalStage normal(normal_display, normal_sink, normal_gate);

    monitor.push({0, 0, false});

    SensorTask task(monitor, normal);
    task.poll(99);

    EXPECT_FALSE(normal.sensor_snapshot().valid);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
