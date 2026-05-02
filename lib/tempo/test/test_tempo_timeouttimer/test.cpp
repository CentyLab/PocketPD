/**
 * GoogleTest suite for tempo::TimeoutTimer.
 */
#include <gtest/gtest.h>
#include <tempo/core/time.h>

using tempo::TimeoutTimer;

TEST(TimeoutTimer, DefaultIsDisarmed) {
    TimeoutTimer d;
    EXPECT_FALSE(d.armed());
    EXPECT_FALSE(d.reached(0));
    EXPECT_FALSE(d.reached(1'000'000));
}

TEST(TimeoutTimer, NotReachedBeforeDuration) {
    TimeoutTimer d;
    d.set(100, 50);
    EXPECT_TRUE(d.armed());
    EXPECT_FALSE(d.reached(100));
    EXPECT_FALSE(d.reached(149));
}

TEST(TimeoutTimer, ReachedAtBoundaryAndStaysReached) {
    TimeoutTimer d;
    d.set(100, 50);
    EXPECT_TRUE(d.reached(150));
    EXPECT_TRUE(d.reached(151));
    EXPECT_TRUE(d.reached(10'000));
}

TEST(TimeoutTimer, DisarmStopsReporting) {
    TimeoutTimer d;
    d.set(0, 100);
    EXPECT_TRUE(d.reached(150));
    d.disarm();
    EXPECT_FALSE(d.armed());
    EXPECT_FALSE(d.reached(150));
}

TEST(TimeoutTimer, ReSetReplacesPriorArm) {
    TimeoutTimer d;
    d.set(0, 100);
    EXPECT_TRUE(d.reached(150));
    d.set(150, 100);
    EXPECT_FALSE(d.reached(200));
    EXPECT_TRUE(d.reached(250));
}

TEST(TimeoutTimer, ZeroDurationIsImmediatelyReached) {
    TimeoutTimer d;
    d.set(500, 0);
    EXPECT_TRUE(d.reached(500));
}

TEST(TimeoutTimer, HandlesUint32Wraparound) {
    TimeoutTimer d;
    constexpr uint32_t near_max = 0xFFFFFFFF - 50;
    d.set(near_max, 100);
    EXPECT_FALSE(d.reached(near_max + 49));
    EXPECT_TRUE(d.reached(near_max + 100));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
