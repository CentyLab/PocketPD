/**
 * GoogleTest suite for BootStage (F1).
 *
 * Exercises splash render via MockDisplay and the BOOT_TO_OBTAIN_MS timeout
 * driving the observable `boot_complete()` flag. Conductor is constructed
 * directly (no full Application) since F1 has no event flow yet.
 */
#define VERSION "\"test\""

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tempo/stage/conductor.h>

#include <MockDisplay.h>

#include "v2/stages/boot_stage.h"

using namespace pocketpd;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::StrEq;

using BootConductor = tempo::Conductor<BootStage>;

TEST(BootStage, OnEnterDrawsSplash) {
    MockDisplay display;
    {
        InSequence seq;
        EXPECT_CALL(display, clear());
        EXPECT_CALL(display, draw_bitmap(0, 0, 16, 64, _));
        EXPECT_CALL(display, draw_text(67, 64, StrEq("FW: ")));
        EXPECT_CALL(display, draw_text(87, 64, _));
        EXPECT_CALL(display, flush());
    }

    BootStage stage(display);
    BootConductor conductor;
    conductor.register_stage(stage);
    conductor.start<BootStage>();
}

TEST(BootStage, CompletesAfterBootTimeout) {
    NiceMock<MockDisplay> display;
    BootStage stage(display);
    BootConductor conductor;
    conductor.register_stage(stage);
    conductor.start<BootStage>();

    EXPECT_FALSE(stage.boot_complete());

    conductor.tick(0);
    EXPECT_FALSE(stage.boot_complete());

    conductor.tick(BOOT_TO_OBTAIN_MS - 1);
    EXPECT_FALSE(stage.boot_complete());

    conductor.tick(BOOT_TO_OBTAIN_MS);
    EXPECT_TRUE(stage.boot_complete());
}

TEST(BootStage, CompletesOnceAcrossMultipleTicks) {
    NiceMock<MockDisplay> display;
    BootStage stage(display);
    BootConductor conductor;
    conductor.register_stage(stage);
    conductor.start<BootStage>();

    conductor.tick(0);
    conductor.tick(BOOT_TO_OBTAIN_MS);
    conductor.tick(BOOT_TO_OBTAIN_MS + 1000);
    EXPECT_TRUE(stage.boot_complete());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
