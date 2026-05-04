/**
 * GoogleTest suite for EncoderTask.
 *
 * Drives the task's public `poll()` directly with a scripted FakeEncoderInput plus a real
 * EventQueue, then inspects what was published.
 */
#define VERSION "\"test\""

#include <MockEncoderInput.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>

#include <variant>

#include "v2/app.h"
#include "v2/events.h"
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

} // namespace

TEST(EncoderTask, OnStartLatchesBaselineWithoutEvent) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    EncoderTask task(enc, pub);

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
    EncoderTask task(enc, pub);
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
    EncoderTask task(enc, pub);
    task.on_start();
    task.poll();

    Event tmp;
    EXPECT_FALSE(q.pop(tmp));
}

TEST(EncoderTask, NegativeDelta) {
    FakeEncoderInput enc;
    TestQueue q;
    TestPublisher pub(q);
    EncoderTask task(enc, pub);
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
