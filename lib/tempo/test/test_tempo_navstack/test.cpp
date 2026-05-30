/**
 * GoogleTest suite for the Conductor navigation stack: request_index, push, pop,
 * replace, reset_root, and the top == current invariant.
 */
#include <variant>

#include <gtest/gtest.h>
#include <tempo/stage/conductor.h>
#include <tempo/stage/stage.h>

namespace {

    using TestEvent = std::variant<std::monostate>;

    class StageA;
    class StageB;
    class StageC;

    using TestConductor = tempo::Conductor<TestEvent, StageA, StageB, StageC>;
    using TestStage = tempo::Stage<TestEvent, StageA, StageB, StageC>;

    class StageA : public TestStage {};
    class StageB : public TestStage {};
    class StageC : public TestStage {};

    struct Fixture {
        StageA a;
        StageB b;
        StageC c;
        TestConductor conductor;
        Fixture() {
            conductor.register_stage(a);
            conductor.register_stage(b);
            conductor.register_stage(c);
        }
    };

} // namespace

TEST(NavStack, PushPopWalksBack) {
    Fixture f;
    f.conductor.start<StageA>(0);
    f.conductor.push<StageB>();
    f.conductor.apply_pending_transition(0);
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageB>());
    f.conductor.push<StageC>();
    f.conductor.apply_pending_transition(0);
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageC>());

    f.conductor.pop();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageB>());
    f.conductor.pop();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageA>());
}

TEST(NavStack, PushCurrentStageIsNoOp) {
    Fixture f;
    f.conductor.start<StageA>(0);
    f.conductor.push<StageA>();
    EXPECT_FALSE(f.conductor.has_pending());
}

TEST(NavStack, PopAtRootIsNoOp) {
    Fixture f;
    f.conductor.start<StageA>(0);
    f.conductor.pop();
    EXPECT_FALSE(f.conductor.has_pending());
}

TEST(NavStack, ReplaceSwapsTopThenPopGoesToRoot) {
    Fixture f;
    f.conductor.start<StageA>(0);
    f.conductor.push<StageB>();
    f.conductor.apply_pending_transition(0);
    f.conductor.replace<StageC>();
    f.conductor.apply_pending_transition(0);
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageC>());
    f.conductor.pop();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageA>());
}

TEST(NavStack, ReplaceWithoutCurrentIsNoOp) {
    Fixture f;
    // No start(): the stack is empty, so there is no top to replace.
    f.conductor.replace<StageB>();
    EXPECT_FALSE(f.conductor.has_pending());
}

TEST(NavStack, ResetRootCollapsesStack) {
    Fixture f;
    f.conductor.start<StageA>(0);
    f.conductor.push<StageB>();
    f.conductor.apply_pending_transition(0);
    f.conductor.push<StageC>();
    f.conductor.apply_pending_transition(0);
    f.conductor.reset_root<StageA>();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageA>());
    f.conductor.pop();
    EXPECT_FALSE(f.conductor.has_pending());
}

TEST(NavStack, ResetPathSeedsStackAndEntersTop) {
    Fixture f;
    f.conductor.start<StageA>(0);
    f.conductor.reset_path<StageA, StageB, StageC>();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageC>());

    // The seeded stages below the top are real back-targets.
    f.conductor.pop();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageB>());
    f.conductor.pop();
    EXPECT_TRUE(f.conductor.apply_pending_transition(0));
    EXPECT_EQ(f.conductor.current_index(), TestConductor::index_of<StageA>());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
