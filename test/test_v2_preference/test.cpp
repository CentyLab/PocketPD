#define VERSION "\"test\""

#include <MockEeprom.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/preferences_store.h"
#include "v2/tasks/preference_task.h"

using namespace pocketpd;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {
    Preferences remember_on() {
        return Preferences{.restore_last_profile_enabled = true};
    }
} // namespace

// —— Updating the store from events

TEST(PreferenceTask, RecordsProfileFromEventWhenRememberOn) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom, remember_on()};
    PreferenceTask task{prefs};

    task.on_event(ActiveProfileEvent{true, 1, 9000, 2000}, 0);

    const LastProfile lp = prefs.get().last_profile;
    EXPECT_TRUE(lp.is_pps);
    EXPECT_EQ(lp.pdo_index, 1);
    EXPECT_EQ(lp.voltage_mv, 9000);
    EXPECT_EQ(lp.current_ma, 2000);
    EXPECT_TRUE(prefs.dirty());
}

TEST(PreferenceTask, IgnoresProfileEventWhenRememberOff) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom}; // remember defaults off
    PreferenceTask task{prefs};

    task.on_event(ActiveProfileEvent{true, 1, 9000, 2000}, 0);

    EXPECT_EQ(prefs.get().last_profile.voltage_mv, 0);
    EXPECT_FALSE(prefs.dirty());
}

TEST(PreferenceTask, IgnoresPassthroughProfileEvent) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom, remember_on()};
    PreferenceTask task{prefs};

    task.on_event(ActiveProfileEvent{false, -1, 0, 0}, 0);

    EXPECT_FALSE(prefs.dirty());
}

TEST(PreferenceTask, IgnoresUnrelatedEvents) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom, remember_on()};
    PreferenceTask task{prefs};

    task.on_event(PpsTargetEvent{1, 9000, 2000}, 0);
    task.on_event(EncoderEvent{1}, 0);

    EXPECT_FALSE(prefs.dirty());
}

// —— Debounced saving

TEST(PreferenceTask, CommitsProfileAfterDebounceQuiet) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom, remember_on()};
    PreferenceTask task{prefs};

    EXPECT_CALL(eeprom, save(_)).Times(1).WillOnce([](const Preferences& p) {
        EXPECT_TRUE(p.last_profile.is_pps);
        EXPECT_EQ(p.last_profile.pdo_index, 1);
        EXPECT_EQ(p.last_profile.voltage_mv, 9000);
        return true;
    });

    task.on_event(ActiveProfileEvent{true, 1, 9000, 2000}, 0);
    task.on_tick(0);                             // notices the change, starts the timer
    task.on_tick(EEPROM_SAVE_DEBOUNCE_MS - 1);   // too soon
    task.on_tick(EEPROM_SAVE_DEBOUNCE_MS);       // fires
    task.on_tick(EEPROM_SAVE_DEBOUNCE_MS + 500); // already clean, no second save
}

TEST(PreferenceTask, NoCommitBeforeDebounce) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom, remember_on()};
    PreferenceTask task{prefs};

    EXPECT_CALL(eeprom, save(_)).Times(0);

    task.on_event(ActiveProfileEvent{true, 1, 9000, 2000}, 0);
    task.on_tick(0);
    task.on_tick(EEPROM_SAVE_DEBOUNCE_MS - 1);
}

TEST(PreferenceTask, LaterChangeRestartsDebounce) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom, remember_on()};
    PreferenceTask task{prefs};

    EXPECT_CALL(eeprom, save(_)).Times(0);

    task.on_event(ActiveProfileEvent{true, 1, 9000, 2000}, 0);
    task.on_tick(0);
    task.on_tick(1500);
    task.on_event(ActiveProfileEvent{true, 1, 9500, 2000}, 1500); // new value
    task.on_tick(1500);                                           // restamps
    task.on_tick(3000);                                           // 1500 < debounce
}

TEST(PreferenceTask, NoCommitWhenNothingChanged) {
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    PreferenceTask task{prefs};

    EXPECT_CALL(eeprom, save(_)).Times(0);

    task.on_tick(0);
    task.on_tick(EEPROM_SAVE_DEBOUNCE_MS + 5000);
}

TEST(PreferenceTask, CommitsAnyDirtyField) {
    // Not profile-specific: a flip_display_enabled change set directly is persisted too.
    NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    PreferenceTask task{prefs};

    EXPECT_CALL(eeprom, save(_)).WillOnce([](const Preferences& p) {
        EXPECT_TRUE(p.flip_display_enabled);
        return true;
    });

    prefs.set({.flip_display_enabled = true});
    task.on_tick(0);
    task.on_tick(EEPROM_SAVE_DEBOUNCE_MS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
