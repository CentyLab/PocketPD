/**
 * GoogleTest suite for CommandTask.
 *
 * Drives a FakeStreamReader through scripted byte sequences and asserts the
 * task echoes complete lines through the attached Logger (CapturingStreamWriter
 * captures the bytes the Logger emits).
 *
 * The Application singleton is constructed once at namespace scope so its
 * `add_task` can attach a real Logger to the task under test.
 */
#define VERSION "\"test\""

#include <MockStreamReader.h>
#include <MockStreamWriter.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/core/time.h>

#include <cstdint>
#include <string>

#include "v2/app.h"
#include "v2/tasks/command_task.h"

using namespace pocketpd;

namespace {

    class FixedClock : public tempo::Clock {
    public:
        uint32_t now = 0;
        uint32_t now_ms() const override {
            return now;
        }
    };

    FixedClock& shared_clock() {
        static FixedClock c;
        return c;
    }

    CapturingStreamWriter& shared_writer() {
        static CapturingStreamWriter w;
        return w;
    }

    /**
     * @brief Build the Application singleton on first access. Tempo only
     * permits one Application per binary, so all CommandTask tests share it.
     */
    App& shared_app() {
        static App app{shared_clock(), shared_writer()};
        return app;
    }

    constexpr uint32_t TICK_MS = 0;

} // namespace

class CommandTaskTest : public ::testing::Test {
protected:
    FakeStreamReader reader;
    CommandTask task{reader, shared_writer()};

    void SetUp() override {
        shared_writer().clear();
        shared_app().add_task(task);
    }
};

TEST_F(CommandTaskTest, EmptySourceProducesNoOutput) {
    task.poll(TICK_MS);
    EXPECT_TRUE(shared_writer().captured.empty());
}

TEST_F(CommandTaskTest, EchoesLineOnNewline) {
    reader.push("hello\n");
    task.poll(TICK_MS);

    EXPECT_NE(shared_writer().captured.find("hello"), std::string::npos);
    EXPECT_EQ(reader.available(), 0);
}

TEST_F(CommandTaskTest, EchoesLineOnCarriageReturn) {
    reader.push("hello\r");
    task.poll(TICK_MS);

    EXPECT_NE(shared_writer().captured.find("hello"), std::string::npos);
    EXPECT_EQ(reader.available(), 0);
}

TEST_F(CommandTaskTest, NoEchoBeforeTerminator) {
    reader.push("hello");
    task.poll(TICK_MS);

    EXPECT_EQ(shared_writer().captured.find("hello"), std::string::npos);
    EXPECT_EQ(reader.available(), 0);
}

TEST_F(CommandTaskTest, PartialLineThenTerminatorEchoesFullLine) {
    reader.push("hel");
    task.poll(TICK_MS);
    EXPECT_EQ(shared_writer().captured.find("hello"), std::string::npos);

    reader.push("lo\n");
    task.poll(TICK_MS);

    EXPECT_NE(shared_writer().captured.find("hello"), std::string::npos);
}

TEST_F(CommandTaskTest, EmptyLinesAreIgnored) {
    reader.push("\n\r\n");
    task.poll(TICK_MS);

    EXPECT_TRUE(shared_writer().captured.empty());
    EXPECT_EQ(reader.available(), 0);
}

TEST_F(CommandTaskTest, MultipleLinesInSinglePoll) {
    reader.push("alpha\nbeta\n");
    task.poll(TICK_MS);

    const std::string& out = shared_writer().captured;
    const auto alpha = out.find("alpha");
    const auto beta = out.find("beta");
    ASSERT_NE(alpha, std::string::npos);
    ASSERT_NE(beta, std::string::npos);
    EXPECT_LT(alpha, beta);
    EXPECT_EQ(reader.available(), 0);
}

TEST_F(CommandTaskTest, OverflowFlushesAndContinues) {
    constexpr size_t MAX_MSG_LEN = 127;
    const std::string overflow(MAX_MSG_LEN + 5, 'x');
    reader.push(overflow);
    reader.push("\n");
    reader.push("next\n");
    task.poll(TICK_MS);

    const std::string& out = shared_writer().captured;
    const auto first_x = out.find('x');
    const auto next_pos = out.find("next");
    ASSERT_NE(first_x, std::string::npos);
    ASSERT_NE(next_pos, std::string::npos);
    EXPECT_LT(first_x, next_pos);
    EXPECT_EQ(reader.available(), 0);
}

TEST_F(CommandTaskTest, TwoSeparatePollsHandleSeparateLines) {
    reader.push("first\n");
    task.poll(TICK_MS);
    const auto first_size = shared_writer().captured.size();

    reader.push("second\n");
    task.poll(TICK_MS);

    EXPECT_NE(shared_writer().captured.find("first"), std::string::npos);
    EXPECT_NE(shared_writer().captured.find("second"), std::string::npos);
    EXPECT_GT(shared_writer().captured.size(), first_size);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
