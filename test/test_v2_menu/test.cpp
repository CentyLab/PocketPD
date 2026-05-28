#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockEeprom.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>

#include "v2/app.h"
#include "v2/preferences_store.h"
#include "v2/events.h"

using namespace pocketpd;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

using TestConductor = App::Conductor;

namespace {

    struct Harness {
        NiceMock<MockDisplay> display;
        NiceMock<MockPdSink> sink;
        NiceMock<MockOutputGate> gate;
        NiceMock<MockEeprom> eeprom;
        PreferencesStore prefs{eeprom};
        MenuStage menu{display};
        ProfilePickerStage picker{display, sink};
        SettingsStage settings_stage{display, prefs};
        NormalStage normal{display, sink, gate};
        TestConductor conductor;

        Harness() {
            conductor.register_stage(menu);
            conductor.register_stage(picker);
            conductor.register_stage(settings_stage);
            conductor.register_stage(normal);
        }
    };

} // namespace

TEST(MenuStage, RendersTwoItemsWithCursorAtTop) {
    Harness h;
    {
        InSequence seq;
        EXPECT_CALL(h.display, clear());
        EXPECT_CALL(h.display, draw_text(0, 12, StrEq(">")));
        EXPECT_CALL(h.display, draw_text(10, 12, StrEq("Profile Picker")));
        EXPECT_CALL(h.display, draw_text(10, 24, StrEq("Settings")));
        EXPECT_CALL(h.display, flush());
    }
    h.conductor.start<MenuStage>(0);
}

TEST(MenuStage, EncoderMovesCursorAndRerenders) {
    Harness h;
    h.conductor.start<MenuStage>(0);
    ::testing::Mock::VerifyAndClearExpectations(&h.display);

    EXPECT_CALL(h.display, clear()).Times(1);
    EXPECT_CALL(h.display, draw_text(10, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(h.display, draw_text(0, 12, StrEq(">"))).Times(0);
    EXPECT_CALL(h.display, draw_text(0, 24, StrEq(">"))).Times(1);
    EXPECT_CALL(h.display, flush()).Times(1);

    h.menu.on_event(h.conductor, EncoderEvent{1}, 0);
}

TEST(MenuStage, EncoderClampsAtTopAndBottom) {
    Harness h;
    h.conductor.start<MenuStage>(0);
    ::testing::Mock::VerifyAndClearExpectations(&h.display);

    EXPECT_CALL(h.display, clear()).Times(0);
    h.menu.on_event(h.conductor, EncoderEvent{-5}, 0);
    ::testing::Mock::VerifyAndClearExpectations(&h.display);

    EXPECT_CALL(h.display, clear()).Times(1);
    h.menu.on_event(h.conductor, EncoderEvent{5}, 0);
    ::testing::Mock::VerifyAndClearExpectations(&h.display);

    EXPECT_CALL(h.display, clear()).Times(0);
    h.menu.on_event(h.conductor, EncoderEvent{1}, 0);
}

TEST(MenuStage, EncoderLongOnProfilePickerRequestsPicker) {
    Harness h;
    h.conductor.start<MenuStage>(0);

    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_TRUE(h.conductor.has_pending());
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
    EXPECT_EQ(h.conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
}

TEST(MenuStage, EncoderLongOnSettingsRequestsSettings) {
    Harness h;
    h.conductor.start<MenuStage>(0);

    h.menu.on_event(h.conductor, EncoderEvent{1}, 0);
    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_TRUE(h.conductor.has_pending());
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
    EXPECT_EQ(h.conductor.current_index(), TestConductor::index_of<SettingsStage>());
}

TEST(MenuStage, LLongReturnsToNormalStage) {
    Harness h;
    EXPECT_CALL(h.sink, is_index_pps(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(h.sink, set_pdo(_)).WillRepeatedly(Return(true));
    h.conductor.start<MenuStage>(0);

    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);

    EXPECT_TRUE(h.conductor.has_pending());
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
    EXPECT_EQ(h.conductor.current_index(), TestConductor::index_of<NormalStage>());
}

TEST(MenuStage, IgnoresUnboundButtonsAndShortPresses) {
    Harness h;
    h.conductor.start<MenuStage>(0);

    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);
    h.menu.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);

    EXPECT_FALSE(h.conductor.has_pending());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
