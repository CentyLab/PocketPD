/**
 * GoogleTest suite for tempo time helpers.
 */
#include <gtest/gtest.h>

#include <tempo/core/time.h>

using tempo::IntervalTimer;

TEST(IntervalTimer, FirstTickArmsAndReturnsFalse) {
    IntervalTimer timer(100);
    EXPECT_FALSE(timer.tick(0));
}

TEST(IntervalTimer, DoesNotFireBeforePeriodElapses) {
    IntervalTimer timer(100);
    timer.tick(0);
    EXPECT_FALSE(timer.tick(50));
    EXPECT_FALSE(timer.tick(99));
}

TEST(IntervalTimer, FiresAtPeriodBoundary) {
    IntervalTimer timer(100);
    timer.tick(0);
    EXPECT_TRUE(timer.tick(100));
}

TEST(IntervalTimer, RearmsAfterFiring) {
    IntervalTimer timer(100);
    timer.tick(0);
    timer.tick(100);
    EXPECT_FALSE(timer.tick(150));
    EXPECT_TRUE(timer.tick(200));
}

TEST(IntervalTimer, FiresOnceAcrossMultipleTicksPastDue) {
    IntervalTimer timer(100);
    timer.tick(0);
    EXPECT_TRUE(timer.tick(250));
    EXPECT_FALSE(timer.tick(300));
    EXPECT_TRUE(timer.tick(350));
}

TEST(IntervalTimer, ResetReArms) {
    IntervalTimer timer(100);
    timer.tick(0);
    timer.tick(100);
    timer.reset(500);
    EXPECT_FALSE(timer.tick(550));
    EXPECT_TRUE(timer.tick(600));
}

TEST(IntervalTimer, HandlesUint32Wraparound) {
    IntervalTimer timer(100);
    constexpr uint32_t near_max = 0xFFFFFFFF - 50;
    timer.tick(near_max);
    EXPECT_TRUE(timer.tick(near_max + 100));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
