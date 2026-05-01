/**
 * GoogleTest suite for BootStage.
 *
 * Exercises splash render via MockDisplay and the BOOT_TO_OBTAIN_MS timeout
 * driving a `request<ObtainStage>()` against the production `Conductor` type
 * list. ObtainStage's slot is left unregistered; the conductor falls back to
 * `NullStage` on apply, which is enough to verify the index transition.
 * ObtainStage's own behaviour is exercised in test_v2_obtain.
 */
#define VERSION "\"test\""

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tempo/stage/conductor.h>

#include <MockDisplay.h>

#include "v2/app.h"
#include "v2/stages/boot_stage.h"

using namespace pocketpd;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::StrEq;

using BootConductor = tempo::Conductor<BootStage, ObtainStage>;

TEST(BootStage, OnEnterDrawsSplash) {
    MockDisplay display;
    {
        InSequence seq;
        EXPECT_CALL(display, clear());
        EXPECT_CALL(display, draw_bitmap(0, 0, 16, 64, ::testing::_));
        EXPECT_CALL(display, draw_text(67, 64, StrEq("FW: ")));
        EXPECT_CALL(display, draw_text(87, 64, ::testing::_));
        EXPECT_CALL(display, flush());
    }

    BootStage stage(display);
    BootConductor conductor;
    conductor.register_stage(stage);
    conductor.start<BootStage>();
}

TEST(BootStage, RequestsObtainAfterBootTimeout) {
    NiceMock<MockDisplay> display;
    BootStage stage(display);
    BootConductor conductor;
    conductor.register_stage(stage);
    conductor.start<BootStage>();

    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(0);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(BOOT_TO_OBTAIN_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(BOOT_TO_OBTAIN_MS);
    EXPECT_TRUE(conductor.has_pending());
}

TEST(BootStage, TransitionsToObtainOnApply) {
    NiceMock<MockDisplay> display;
    BootStage stage(display);
    BootConductor conductor;
    conductor.register_stage(stage);
    conductor.start<BootStage>();

    conductor.tick(0);
    conductor.tick(BOOT_TO_OBTAIN_MS);

    EXPECT_TRUE(conductor.apply_pending_transition());
    EXPECT_EQ(conductor.current_index(), BootConductor::index_of<ObtainStage>());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
