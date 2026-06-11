#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockDisplayOrientation.h>
#include <MockEeprom.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>

#include "v2/app.h"
#include "v2/preferences_store.h"
#include "v2/events.h"
#include "v2/hal/eeprom.h"

using namespace pocketpd;
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
        FakeDisplayOrientation orientation;
        PreferencesStore prefs{eeprom};
        SettingsStage stage{display, orientation, prefs};
        MenuStage menu{display};
        ProfilePickerStage picker{display, sink};
        NormalStage normal{display, sink, gate};
        TestConductor conductor;

        Harness() {
            conductor.register_stage(stage);
            conductor.register_stage(menu);
            conductor.register_stage(picker);
            conductor.register_stage(normal);
        }
    };

} // namespace

TEST(SettingsStage, RendersOneRowWithUncheckedBox) {
    Harness h;
    EXPECT_CALL(h.display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(h.display, draw_text(0, 12, StrEq(">"))).Times(1);
    EXPECT_CALL(h.display, draw_text(10, 12, StrEq("[ ] Skip picker"))).Times(1);
    h.conductor.start<SettingsStage>(0);
}

TEST(SettingsStage, RendersCheckedWhenSettingTrue) {
    Harness h;
    h.prefs.set({.skip_picker_on_boot = true});
    EXPECT_CALL(h.display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(h.display, draw_text(10, 12, StrEq("[X] Skip picker"))).Times(1);
    h.conductor.start<SettingsStage>(0);
}

TEST(SettingsStage, EncoderLongTogglesInRamWithoutSaving) {
    Harness h;
    h.conductor.start<SettingsStage>(0);

    EXPECT_CALL(h.eeprom, save(_)).Times(0);

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    EXPECT_TRUE(h.prefs.get().skip_picker_on_boot);
    EXPECT_TRUE(h.prefs.dirty());
}

TEST(SettingsStage, ExitFlushesPendingToggleToEeprom) {
    Harness h;
    h.conductor.start<MenuStage>(0);
    h.conductor.push<SettingsStage>();
    h.conductor.apply_pending_transition(0);

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_CALL(h.eeprom, save(_))
        .WillOnce([](const Preferences& s) {
            EXPECT_TRUE(s.skip_picker_on_boot);
            return true;
        });

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
}

TEST(SettingsStage, MultipleTogglesCollapseToSingleSaveOnExit) {
    Harness h;
    h.conductor.start<MenuStage>(0);
    h.conductor.push<SettingsStage>();
    h.conductor.apply_pending_transition(0);

    EXPECT_CALL(h.eeprom, save(_)).Times(1).WillOnce(Return(true));

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    EXPECT_TRUE(h.prefs.get().skip_picker_on_boot);

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
}

TEST(SettingsStage, ExitWithoutTogglesDoesNotSave) {
    Harness h;
    h.conductor.start<MenuStage>(0);
    h.conductor.push<SettingsStage>();
    h.conductor.apply_pending_transition(0);

    EXPECT_CALL(h.eeprom, save(_)).Times(0);

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
    EXPECT_EQ(h.conductor.current_index(), TestConductor::index_of<MenuStage>());
}

TEST(SettingsStage, RendersVoltageCompRowBelowSkipPicker) {
    Harness h;
    EXPECT_CALL(h.display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(h.display, draw_text(10, 12, StrEq("[ ] Skip picker"))).Times(1);
    EXPECT_CALL(h.display, draw_text(10, 24, StrEq("[ ] Voltage comp"))).Times(1);
    h.conductor.start<SettingsStage>(0);
}

TEST(SettingsStage, EncoderMovesCursorToVoltageCompRow) {
    Harness h;
    h.conductor.start<SettingsStage>(0);
    ::testing::Mock::VerifyAndClearExpectations(&h.display);

    EXPECT_CALL(h.display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(h.display, draw_text(0, 24, StrEq(">"))).Times(1);
    EXPECT_CALL(h.display, draw_text(0, 12, StrEq(">"))).Times(0);

    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
}

TEST(SettingsStage, EncoderLongOnVoltageCompTogglesPreference) {
    Harness h;
    h.conductor.start<SettingsStage>(0);

    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_TRUE(h.prefs.get().voltage_comp_enabled);
    EXPECT_TRUE(h.prefs.dirty());
}

TEST(SettingsStage, ExitFlushesVoltageCompToggle) {
    Harness h;
    h.conductor.start<MenuStage>(0);
    h.conductor.push<SettingsStage>();
    h.conductor.apply_pending_transition(0);

    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_CALL(h.eeprom, save(_))
        .WillOnce([](const Preferences& s) {
            EXPECT_TRUE(s.voltage_comp_enabled);
            EXPECT_FALSE(s.skip_picker_on_boot);
            return true;
        });

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);
    EXPECT_TRUE(h.conductor.apply_pending_transition(0));
}

TEST(SettingsStage, RendersFlipDisplayRowBelowVoltageComp) {
    Harness h;
    EXPECT_CALL(h.display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(h.display, draw_text(10, 36, StrEq("[ ] Flip display"))).Times(1);
    h.conductor.start<SettingsStage>(0);
}

TEST(SettingsStage, EncoderLongOnFlipDisplayTogglesPreferenceAndApplies) {
    Harness h;
    h.conductor.start<SettingsStage>(0);

    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_TRUE(h.prefs.get().flip_display);
    EXPECT_TRUE(h.prefs.dirty());
    EXPECT_TRUE(h.orientation.flipped());
    EXPECT_EQ(h.orientation.call_count(), 1);
}

TEST(SettingsStage, FlipDisplayToggleTwiceRestoresOrientation) {
    Harness h;
    h.conductor.start<SettingsStage>(0);

    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
    h.stage.on_event(h.conductor, EncoderEvent{1}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);
    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_FALSE(h.prefs.get().flip_display);
    EXPECT_FALSE(h.orientation.flipped());
    EXPECT_EQ(h.orientation.call_count(), 2);
}

TEST(SettingsStage, TogglingOtherItemsDoesNotTouchOrientation) {
    Harness h;
    h.conductor.start<SettingsStage>(0);

    h.stage.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::LONG}, 0);

    EXPECT_EQ(h.orientation.call_count(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
