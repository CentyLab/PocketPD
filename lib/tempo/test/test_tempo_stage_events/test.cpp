/**
 * GoogleTest suite for Stage::on_event first-class dispatch.
 *
 * Verifies Conductor::dispatch_event delivers the event to the active stage's
 * on_event, leaves inactive stages untouched, and that on_event sees the same
 * Conductor reference as the rest of the lifecycle callbacks.
 */
#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>
#include <tempo/stage/stage.h>

#include <variant>

namespace {

    struct Ping {
        int n = 0;
    };
    struct Pong {
        int n = 0;
    };
    using TestEvent = std::variant<std::monostate, Ping, Pong>;

    class StageA;
    class StageB;

    using TestConductor = tempo::Conductor<TestEvent, StageA, StageB>;
    using TestStage = tempo::Stage<TestEvent, StageA, StageB>;

    class StageA : public TestStage {
    public:
        int ping_count = 0;
        int pong_count = 0;
        TestConductor* seen_conductor = nullptr;
        uint32_t last_now_ms = 0;

        const char* name() const override {
            return "A";
        }

        void on_event(TestConductor& conductor, const TestEvent& event, uint32_t now_ms) override {
            seen_conductor = &conductor;
            last_now_ms = now_ms;
            if (std::holds_alternative<Ping>(event)) {
                ++ping_count;
            } else if (std::holds_alternative<Pong>(event)) {
                ++pong_count;
            }
        }
    };

    class StageB : public TestStage {
    public:
        int events_seen = 0;

        const char* name() const override {
            return "B";
        }

        void on_event(TestConductor&, const TestEvent&, uint32_t) override {
            ++events_seen;
        }
    };

} // namespace

TEST(StageEvents, DispatchDeliversEventToActiveStageOnly) {
    StageA a;
    StageB b;
    TestConductor c;
    c.register_stage(a);
    c.register_stage(b);
    c.start<StageA>();

    c.dispatch_event(TestEvent{Ping{42}}, 1000);

    EXPECT_EQ(a.ping_count, 1);
    EXPECT_EQ(a.pong_count, 0);
    EXPECT_EQ(b.events_seen, 0);
    EXPECT_EQ(a.seen_conductor, &c);
    EXPECT_EQ(a.last_now_ms, 1000u);
}

TEST(StageEvents, DispatchAfterTransitionReachesNewActiveStage) {
    StageA a;
    StageB b;
    TestConductor c;
    c.register_stage(a);
    c.register_stage(b);
    c.start<StageA>();

    c.request<StageB>();
    c.apply_pending_transition();

    c.dispatch_event(TestEvent{Ping{1}}, 2000);

    EXPECT_EQ(a.ping_count, 0);
    EXPECT_EQ(b.events_seen, 1);
}

TEST(StageEvents, MultipleEventsAccumulateOnActiveStage) {
    StageA a;
    StageB b;
    TestConductor c;
    c.register_stage(a);
    c.register_stage(b);
    c.start<StageA>();

    c.dispatch_event(TestEvent{Ping{1}}, 100);
    c.dispatch_event(TestEvent{Pong{2}}, 200);
    c.dispatch_event(TestEvent{Ping{3}}, 300);

    EXPECT_EQ(a.ping_count, 2);
    EXPECT_EQ(a.pong_count, 1);
    EXPECT_EQ(a.last_now_ms, 300u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
