/**
 * GoogleTest suite for ButtonGestureDetector, ButtonTask, EncoderTask.
 *
 * Detector is exercised in isolation since it is pure logic. ButtonTask tests cover the wiring:
 * three FakeButtonInputs feed three detectors, gestures land on the EventQueue tagged with the
 * right ButtonId.
 */
#define VERSION "\"test\""

#include <MockButtonInput.h>
#include <MockEeprom.h>
#include <MockEncoderInput.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>

#include <variant>

#include "v2/events.h"
#include "v2/input/button_gesture.h"
#include "v2/input/two_buttons_gesture.h"
#include "v2/pocketpd.h"
#include "v2/preferences_store.h"
#include "v2/tasks/button_task.h"
#include "v2/tasks/encoder_task.h"

using namespace pocketpd;

using TestQueue = tempo::EventQueue<Event, 8>;
using TestPublisher = tempo::QueuePublisher<Event, 8>;

namespace {

    template <typename T>
    const T* pop_as(TestQueue& q) {
        static Event last;
        if (!q.pop(last)) {
            return nullptr;
        }
        return std::get_if<T>(&last);
    }

    constexpr ButtonGestureConfig kDefaultCfg{};

} // namespace

// —— ButtonId

TEST(ButtonId, LRComboValueIsDistinct) {
    EXPECT_NE(static_cast<int>(ButtonId::L_R), static_cast<int>(ButtonId::L));
    EXPECT_NE(static_cast<int>(ButtonId::L_R), static_cast<int>(ButtonId::R));
    EXPECT_NE(static_cast<int>(ButtonId::L_R), static_cast<int>(ButtonId::ENCODER));
}

// —— ButtonGestureDetector

TEST(ButtonGestureDetector, ShortFiresOnRelease) {
    ButtonGestureDetector d;
    EXPECT_FALSE(d.update(0, true).has_value());

    auto g = d.update(50, false);
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(*g, Gesture::SHORT);
}

TEST(ButtonGestureDetector, LongFiresAtThresholdWhileHeld) {
    ButtonGestureDetector d;
    EXPECT_FALSE(d.update(0, true).has_value());
    EXPECT_FALSE(d.update(kDefaultCfg.long_press_ms - 1, true).has_value());

    auto g = d.update(kDefaultCfg.long_press_ms, true);
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(*g, Gesture::LONG);

    EXPECT_FALSE(d.update(kDefaultCfg.long_press_ms + 500, true).has_value());
    EXPECT_FALSE(d.update(kDefaultCfg.long_press_ms + 600, false).has_value());
}

TEST(ButtonGestureDetector, ConsecutivePressesEachEmitShort) {
    ButtonGestureDetector d;
    d.update(0, true);
    auto first = d.update(30, false);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, Gesture::SHORT);

    d.update(100, true);
    auto second = d.update(130, false);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, Gesture::SHORT);
}

// —— ButtonTask

TEST(ButtonTask, ShortGestureOnQuickRelease) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    encoder.set_held(true);
    task.poll(0);
    encoder.set_held(false);
    task.poll(100);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::ENCODER);
    EXPECT_EQ(btn->gesture, Gesture::SHORT);
}

TEST(ButtonTask, LongGestureFiresWhileHeldAndSilencesRelease) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    encoder.set_held(true);
    task.poll(0);
    task.poll(kDefaultCfg.long_press_ms - 1);
    Event tmp;
    EXPECT_FALSE(q.pop(tmp));

    task.poll(kDefaultCfg.long_press_ms);
    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->gesture, Gesture::LONG);

    encoder.set_held(false);
    task.poll(kDefaultCfg.long_press_ms + 100);
    EXPECT_FALSE(q.pop(tmp));
}

TEST(ButtonTask, RButtonShortGestureRoutesToR) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    r.set_held(true);
    task.poll(0);
    r.set_held(false);
    task.poll(50);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::R);
    EXPECT_EQ(btn->gesture, Gesture::SHORT);
}

TEST(ButtonTask, LButtonLongPress) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    l.set_held(true);
    task.poll(0);
    task.poll(kDefaultCfg.long_press_ms);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::L);
    EXPECT_EQ(btn->gesture, Gesture::LONG);
}

// —— ButtonTask flipped display

TEST(ButtonTask, FlipDisplaySwapsPublishedLR) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    prefs.set({.flip_display = true});

    l.set_held(true);
    task.poll(0);
    l.set_held(false);
    task.poll(50);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::R);

    r.set_held(true);
    task.poll(200);
    r.set_held(false);
    task.poll(250);

    btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::L);
}

TEST(ButtonTask, FlipDisplayLeavesEncoderAndComboAlone) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    prefs.set({.flip_display = true});

    encoder.set_held(true);
    task.poll(0);
    encoder.set_held(false);
    task.poll(50);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::ENCODER);

    l.set_held(true);
    r.set_held(true);
    task.poll(200);
    task.poll(200 + kDefaultCfg.long_press_ms);

    btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::L_R);
}

// —— EncoderTask

TEST(EncoderTask, OnStartLatchesBaselineWithoutEvent) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    EncoderTask task(enc, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    enc.set_position(42);
    task.on_start();
    task.poll();

    Event tmp;
    EXPECT_FALSE(q.pop(tmp));
}

TEST(EncoderTask, NonZeroDeltaPublishes) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    EncoderTask task(enc, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);
    task.on_start();

    enc.set_position(3);
    task.poll();

    const auto* ev = pop_as<EncoderEvent>(q);
    ASSERT_NE(ev, nullptr);
    EXPECT_EQ(ev->delta, 3);
}

TEST(EncoderTask, NoChangeMeansNoEvent) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    EncoderTask task(enc, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);
    task.on_start();
    task.poll();

    Event tmp;
    EXPECT_FALSE(q.pop(tmp));
}

TEST(EncoderTask, NegativeDelta) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    EncoderTask task(enc, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);
    enc.set_position(5);
    task.on_start();
    enc.set_position(2);
    task.poll();

    const auto* ev = pop_as<EncoderEvent>(q);
    ASSERT_NE(ev, nullptr);
    EXPECT_EQ(ev->delta, -3);
}

TEST(EncoderTask, FlipDisplayNegatesDelta) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    EncoderTask task(enc, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    prefs.set({.flip_display = true});

    task.on_start();
    enc.set_position(3);
    task.poll();

    const auto* ev = pop_as<EncoderEvent>(q);
    ASSERT_NE(ev, nullptr);
    EXPECT_EQ(ev->delta, -3);
}

// —— ButtonTask combo

TEST(ButtonTask, BriefSimultaneousTapDropsBothShorts) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    l.set_held(true);
    r.set_held(true);
    task.poll(0);
    task.poll(50);
    l.set_held(false);
    r.set_held(false);
    task.poll(100);

    Event tmp;
    EXPECT_FALSE(q.pop(tmp));
}

TEST(ButtonTask, ComboLongAtThresholdEmitsLRSuppressesIndividuals) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    l.set_held(true);
    r.set_held(true);
    task.poll(0);
    task.poll(kDefaultCfg.long_press_ms - 1);
    Event mid;
    EXPECT_FALSE(q.pop(mid));

    task.poll(kDefaultCfg.long_press_ms);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::L_R);
    EXPECT_EQ(btn->gesture, Gesture::LONG);

    Event leftover;
    EXPECT_FALSE(q.pop(leftover));

    l.set_held(false);
    r.set_held(false);
    task.poll(kDefaultCfg.long_press_ms + 100);
    EXPECT_FALSE(q.pop(leftover));
}

TEST(ButtonTask, AbortedComboCancelsRemainingSingles) {
    // Once a combo arms, the chord lifetime owns L+R suppression until BOTH are released.
    // A failed combo attempt (one finger lifted early) must not leak a single LONG on the
    // finger that is still held — releasing both is required before per-finger gestures
    // resume.
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    l.set_held(true);
    task.poll(0);
    r.set_held(true);
    task.poll(200);
    r.set_held(false);
    task.poll(240);

    Event before;
    EXPECT_FALSE(q.pop(before));

    task.poll(kDefaultCfg.long_press_ms);
    Event leftover;
    EXPECT_FALSE(q.pop(leftover));

    l.set_held(false);
    task.poll(kDefaultCfg.long_press_ms + 50);
    EXPECT_FALSE(q.pop(leftover));
}

TEST(ButtonTask, EncoderButtonNotSuppressedByCombo) {
    FakeButtonInput encoder, l, r;
    TestQueue q;
    TestPublisher pub(q);
    ::testing::NiceMock<MockEeprom> eeprom;
    PreferencesStore prefs{eeprom};
    ButtonTask task(encoder, l, r, prefs);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    l.set_held(true);
    r.set_held(true);
    task.poll(0);

    encoder.set_held(true);
    task.poll(20);
    encoder.set_held(false);
    task.poll(70);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::ENCODER);
    EXPECT_EQ(btn->gesture, Gesture::SHORT);
}

// —— TwoButtonsGestureDetector

TEST(TwoButtonsGestureDetector, IdleUntilBothHeld) {
    TwoButtonsGestureDetector d;
    auto out = d.update(0, false, false);
    EXPECT_FALSE(d.is_active());
    EXPECT_FALSE(out.has_value());
}

TEST(TwoButtonsGestureDetector, OneAloneStaysIdle) {
    TwoButtonsGestureDetector d;
    auto out = d.update(0, true, false);
    EXPECT_FALSE(d.is_active());
    EXPECT_FALSE(out.has_value());
}

TEST(TwoButtonsGestureDetector, ArmsWhenBothHeld) {
    TwoButtonsGestureDetector d;
    auto out = d.update(100, true, true);
    EXPECT_TRUE(d.is_active());
    EXPECT_FALSE(out.has_value());
}

TEST(TwoButtonsGestureDetector, FiresLongAtThreshold) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    auto mid = d.update(kDefaultCfg.long_press_ms - 1, true, true);
    EXPECT_TRUE(d.is_active());
    EXPECT_FALSE(mid.has_value());

    auto fire = d.update(kDefaultCfg.long_press_ms, true, true);
    EXPECT_TRUE(d.is_active());
    ASSERT_TRUE(fire.has_value());
    EXPECT_EQ(*fire, Gesture::LONG);
}

TEST(TwoButtonsGestureDetector, NoSecondEmitWhileStillHeldAfterFire) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    d.update(kDefaultCfg.long_press_ms, true, true);
    auto out = d.update(kDefaultCfg.long_press_ms + 200, true, true);
    EXPECT_TRUE(d.is_active());
    EXPECT_FALSE(out.has_value());
}

TEST(TwoButtonsGestureDetector, AbortsWhenOneReleasedBeforeThreshold) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    auto out = d.update(200, true, false);
    EXPECT_TRUE(d.is_active());
    EXPECT_FALSE(out.has_value());
}

TEST(TwoButtonsGestureDetector, ReleaseExactlyAtThresholdAborts) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    auto out = d.update(kDefaultCfg.long_press_ms, true, false);
    EXPECT_TRUE(d.is_active());
    EXPECT_FALSE(out.has_value());
}

TEST(TwoButtonsGestureDetector, AbortedPersistsWhileOneHeld) {
    // LATCHED must stick until BOTH buttons release, so callers can use
    // `is_active()` as the L/R suppression predicate for the whole chord lifetime.
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    d.update(100, true, false);
    ASSERT_TRUE(d.is_active());

    d.update(200, true, false);
    EXPECT_TRUE(d.is_active());
}

TEST(TwoButtonsGestureDetector, AbortClearsOnlyAfterBothReleased) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    d.update(100, true, false);  // latched (aborted)
    d.update(110, true, false);
    EXPECT_TRUE(d.is_active());

    d.update(200, false, false);
    EXPECT_FALSE(d.is_active());

    auto rearm = d.update(300, true, true);
    EXPECT_TRUE(d.is_active());
    EXPECT_FALSE(rearm.has_value());
}

TEST(TwoButtonsGestureDetector, FiredPersistsWhileOneHeld) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    d.update(kDefaultCfg.long_press_ms, true, true);  // fired → LATCHED
    d.update(kDefaultCfg.long_press_ms + 50, true, false);
    EXPECT_TRUE(d.is_active());
}

TEST(TwoButtonsGestureDetector, FiredClearsOnlyAfterBothReleased) {
    TwoButtonsGestureDetector d;
    d.update(0, true, true);
    d.update(kDefaultCfg.long_press_ms, true, true);
    d.update(kDefaultCfg.long_press_ms + 50, false, false);
    EXPECT_FALSE(d.is_active());
}

TEST(TwoButtonsGestureDetector, SecondPressArmsAtSecondPressTime) {
    // L pressed at t=0, R pressed at t=80; chord timer should start at t=80
    // and fire at t=80 + long_press_ms.
    TwoButtonsGestureDetector d;
    d.update(0, true, false);   // only L held
    d.update(80, true, true);
    EXPECT_TRUE(d.is_active());

    auto pre = d.update(80 + kDefaultCfg.long_press_ms - 1, true, true);
    EXPECT_FALSE(pre.has_value());

    auto fire = d.update(80 + kDefaultCfg.long_press_ms, true, true);
    ASSERT_TRUE(fire.has_value());
    EXPECT_EQ(*fire, Gesture::LONG);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
