/**
 * GoogleTest suite for ObtainStage.
 */
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
#include "v2/hal/eeprom.h"
#include "v2/stages/boot_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/obtain_stage.h"
#include "v2/stages/profile_picker_stage.h"

using namespace pocketpd;
using ::testing::NiceMock;
using ::testing::Return;

using TestConductor = App::Conductor;

TEST(ObtainStage, OnEnterCallsBegin) {
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore default_prefs{mock_eeprom};

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));

    ObtainStage stage(sink, default_prefs);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>(0);
}

TEST(ObtainStage, OnEnterIssuesNoRdoRequest) {
    // Issue #33: charger-dependent default voltage. Obtain must not pick a PDO.
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore default_prefs{mock_eeprom};

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    ObtainStage stage(sink, default_prefs);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.start<ObtainStage>(0);
}

TEST(ObtainStage, ShortButtonJumpsToProfilePicker) {
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore default_prefs{mock_eeprom};
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));

    ObtainStage stage(sink, default_prefs);
    NiceMock<MockDisplay> picker_display;
    ProfilePickerStage picker(picker_display, sink);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(picker);
    conductor.start<ObtainStage>(0);

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
}

TEST(ObtainStage, EncoderRotationJumpsToProfilePicker) {
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore default_prefs{mock_eeprom};
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));

    ObtainStage stage(sink, default_prefs);
    NiceMock<MockDisplay> picker_display;
    ProfilePickerStage picker(picker_display, sink);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(picker);
    conductor.start<ObtainStage>(0);

    stage.on_event(conductor, EncoderEvent{-1}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
}

TEST(ObtainStage, TimeoutTransitionsToProfilePicker) {
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore default_prefs{mock_eeprom};
    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    ObtainStage stage(sink, default_prefs);
    NiceMock<MockDisplay> picker_display;
    ProfilePickerStage picker(picker_display, sink);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(picker);
    conductor.start<ObtainStage>(0);

    conductor.tick(0);
    conductor.tick(OBTAIN_TO_PROFILE_PICKER_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(OBTAIN_TO_PROFILE_PICKER_MS);
    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
}

TEST(BootStage, RequestsObtainAfterTimeout) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore default_prefs{mock_eeprom};

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    BootStage boot(display);
    ObtainStage obtain(sink, default_prefs);
    TestConductor conductor;
    conductor.register_stage(boot);
    conductor.register_stage(obtain);
    conductor.start<BootStage>(0);

    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<BootStage>());

    conductor.tick(0);
    conductor.tick(BOOT_TO_OBTAIN_MS - 1);
    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(BOOT_TO_OBTAIN_MS);
    EXPECT_TRUE(conductor.has_pending());

    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ObtainStage>());
}

TEST(ObtainStage, SkipPickerOnBootRequestsNormal) {
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore config{mock_eeprom, Preferences{.skip_picker_on_boot = true}};
    NiceMock<MockDisplay> display;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    ObtainStage stage(sink, config);
    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(normal);
    conductor.start<ObtainStage>(0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
}

TEST(ObtainStage, SkipPickerDisabledFollowsNormalTimeoutPath) {
    NiceMock<MockPdSink> sink;
    NiceMock<MockEeprom> mock_eeprom;
    PreferencesStore config{mock_eeprom};
    NiceMock<MockDisplay> display;

    EXPECT_CALL(sink, begin()).WillOnce(Return(true));
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(2));

    ObtainStage stage(sink, config);
    ProfilePickerStage picker(display, sink);
    TestConductor conductor;
    conductor.register_stage(stage);
    conductor.register_stage(picker);
    conductor.start<ObtainStage>(0);

    EXPECT_FALSE(conductor.has_pending());

    conductor.tick(OBTAIN_TO_PROFILE_PICKER_MS);
    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<ProfilePickerStage>());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
