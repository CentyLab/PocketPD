/**
 * GoogleTest suite for `Conductor::request<S>(Args...)` payload-passing transitions.
 *
 * Verifies the payload form forwards arguments to `S::prepare(...)` immediately
 * (before the transition is applied) and that stages without prepare still
 * accept the zero-arg `request<S>()` form unchanged.
 */
#include <gtest/gtest.h>
#include <tempo/bus/event.h>
#include <tempo/stage/conductor.h>
#include <tempo/stage/stage.h>

namespace {

    enum class Mode { REVIEW, SELECT };

    class StageWithPayload;
    class PlainStage;

    using TestEvent = tempo::Events<>;
    using TestConductor = tempo::Conductor<TestEvent, StageWithPayload, PlainStage>;
    using TestStage = tempo::Stage<TestEvent, StageWithPayload, PlainStage>;

    class StageWithPayload : public TestStage {
    public:
        Mode last_prepared = Mode::REVIEW;
        int prepare_call_count = 0;
        Mode mode_seen_in_on_enter = Mode::REVIEW;
        int on_enter_count = 0;

        const char* name() const override {
            return "WithPayload";
        }

        void prepare(Mode m) {
            last_prepared = m;
            ++prepare_call_count;
        }

        void on_enter(TestConductor&) override {
            mode_seen_in_on_enter = last_prepared;
            ++on_enter_count;
        }
    };

    class PlainStage : public TestStage {
    public:
        int on_enter_count = 0;

        const char* name() const override {
            return "Plain";
        }

        void on_enter(TestConductor&) override {
            ++on_enter_count;
        }
    };

} // namespace

TEST(RequestPayload, RequestWithoutArgsLeavesStageDefaults) {
    StageWithPayload payload;
    PlainStage plain;
    TestConductor c;
    c.register_stage(payload);
    c.register_stage(plain);
    c.start<PlainStage>();

    c.request<StageWithPayload>();
    EXPECT_EQ(payload.prepare_call_count, 0);

    c.apply_pending_transition();
    EXPECT_EQ(payload.on_enter_count, 1);
    EXPECT_EQ(payload.mode_seen_in_on_enter, Mode::REVIEW);
}

TEST(RequestPayload, RequestWithArgCallsPrepareImmediately) {
    StageWithPayload payload;
    PlainStage plain;
    TestConductor c;
    c.register_stage(payload);
    c.register_stage(plain);
    c.start<PlainStage>();

    c.request<StageWithPayload>(Mode::SELECT);

    // prepare runs at request time, before apply.
    EXPECT_EQ(payload.prepare_call_count, 1);
    EXPECT_EQ(payload.last_prepared, Mode::SELECT);
    EXPECT_EQ(payload.on_enter_count, 0);

    c.apply_pending_transition();
    EXPECT_EQ(payload.on_enter_count, 1);
    EXPECT_EQ(payload.mode_seen_in_on_enter, Mode::SELECT);
}

TEST(RequestPayload, LatestRequestWins) {
    StageWithPayload payload;
    PlainStage plain;
    TestConductor c;
    c.register_stage(payload);
    c.register_stage(plain);
    c.start<PlainStage>();

    c.request<StageWithPayload>(Mode::REVIEW);
    c.request<StageWithPayload>(Mode::SELECT);
    EXPECT_EQ(payload.prepare_call_count, 2);

    c.apply_pending_transition();
    EXPECT_EQ(payload.mode_seen_in_on_enter, Mode::SELECT);
}

TEST(RequestPayload, PlainStageStillTransitions) {
    StageWithPayload payload;
    PlainStage plain;
    TestConductor c;
    c.register_stage(payload);
    c.register_stage(plain);
    c.start<StageWithPayload>();

    c.request<PlainStage>();
    c.apply_pending_transition();
    EXPECT_EQ(plain.on_enter_count, 1);
}

TEST(RequestPayload, SameStageRequestReFiresLifecycleWithNewPayload) {
    StageWithPayload payload;
    PlainStage plain;
    TestConductor c;
    c.register_stage(payload);
    c.register_stage(plain);

    payload.prepare(Mode::REVIEW);
    c.start<StageWithPayload>();
    EXPECT_EQ(payload.on_enter_count, 1);
    EXPECT_EQ(payload.mode_seen_in_on_enter, Mode::REVIEW);

    // Same-stage request with a different payload should re-fire on_enter.
    c.request<StageWithPayload>(Mode::SELECT);
    EXPECT_TRUE(c.apply_pending_transition());
    EXPECT_EQ(payload.on_enter_count, 2);
    EXPECT_EQ(payload.mode_seen_in_on_enter, Mode::SELECT);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
