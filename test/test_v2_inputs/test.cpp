/**
 * GoogleTest suite for ButtonGestureDetector, ButtonTask, EncoderTask.
 *
 * Detector is exercised in isolation since it is pure logic. ButtonTask tests cover the wiring:
 * three FakeButtonInputs feed three detectors, gestures land on the EventQueue tagged with the
 * right ButtonId.
 */
#define VERSION "\"test\""

#include <MockButtonInput.h>
#include <MockEncoderInput.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>

#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/input/button_gesture.h"
#include "v2/state.h"
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

// —— ButtonGestureDetector

TEST(ButtonGestureDetector, ShortFiresOnRelease) {
    ButtonGestureDetector d;
    EXPECT_FALSE(d.update(true, 0).has_value());

    auto g = d.update(false, 50);
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(*g, Gesture::SHORT);
}

TEST(ButtonGestureDetector, LongFiresAtThresholdWhileHeld) {
    ButtonGestureDetector d;
    EXPECT_FALSE(d.update(true, 0).has_value());
    EXPECT_FALSE(d.update(true, kDefaultCfg.long_press_ms - 1).has_value());

    auto g = d.update(true, kDefaultCfg.long_press_ms);
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(*g, Gesture::LONG);

    EXPECT_FALSE(d.update(true, kDefaultCfg.long_press_ms + 500).has_value());
    EXPECT_FALSE(d.update(false, kDefaultCfg.long_press_ms + 600).has_value());
}

TEST(ButtonGestureDetector, ConsecutivePressesEachEmitShort) {
    ButtonGestureDetector d;
    d.update(true, 0);
    auto first = d.update(false, 30);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, Gesture::SHORT);

    d.update(true, 100);
    auto second = d.update(false, 130);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, Gesture::SHORT);
}

// —— ButtonTask

TEST(ButtonTask, ShortGestureOnQuickRelease) {
    FakeButtonInput encoder, vi_selector, output;
    TestQueue q;
    TestPublisher pub(q);
    ButtonTask task(encoder, vi_selector, output);
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
    FakeButtonInput encoder, vi_selector, output;
    TestQueue q;
    TestPublisher pub(q);
    ButtonTask task(encoder, vi_selector, output);
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

TEST(ButtonTask, OutputButtonShortGestureRoutesToOutputToggle) {
    FakeButtonInput encoder, vi_selector, output;
    TestQueue q;
    TestPublisher pub(q);
    ButtonTask task(encoder, vi_selector, output);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    output.set_held(true);
    task.poll(0);
    output.set_held(false);
    task.poll(50);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::OUTPUT_TOGGLE);
    EXPECT_EQ(btn->gesture, Gesture::SHORT);
}

TEST(ButtonTask, SelectViLongPress) {
    FakeButtonInput encoder, vi_selector, output;
    TestQueue q;
    TestPublisher pub(q);
    ButtonTask task(encoder, vi_selector, output);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    vi_selector.set_held(true);
    task.poll(0);
    task.poll(kDefaultCfg.long_press_ms);

    const auto* btn = pop_as<ButtonEvent>(q);
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->id, ButtonId::SELECT_VI);
    EXPECT_EQ(btn->gesture, Gesture::LONG);
}

// —— EncoderTask

TEST(EncoderTask, OnStartLatchesBaselineWithoutEvent) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    EncoderTask task(enc);
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
    EncoderTask task(enc);
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
    EncoderTask task(enc);
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
    EncoderTask task(enc);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);
    enc.set_position(5);
    task.on_start();
    enc.set_position(2);
    task.poll();

    const auto* ev = pop_as<EncoderEvent>(q);
    ASSERT_NE(ev, nullptr);
    EXPECT_EQ(ev->delta, -3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
