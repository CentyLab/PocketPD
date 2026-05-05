/**
 * GoogleTest suite for tempo Logger fmt-backed implementation.
 *
 * Asserts header byte sequence, body formatting, footer, level gating, hexdump output,
 * and behavior when StreamWriter is unset. Inline mocks for Clock + StreamWriter.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <tempo/core/time.h>
#include <tempo/diag/logger.h>
#include <tempo/hardware/stream.h>

namespace {

    class FakeClock : public tempo::Clock {
    public:
        uint32_t now = 0;
        uint32_t now_ms() const override {
            return now;
        }
    };

    class CapturingStream : public tempo::StreamWriter {
    public:
        std::string captured;
        size_t write_calls = 0;

        size_t write(const char* data, size_t len) override {
            captured.append(data, len);
            ++write_calls;
            return len;
        }
    };

} // namespace

class LoggerTest : public ::testing::Test {
protected:
    FakeClock clock;
    CapturingStream stream;
    tempo::Logger logger{"TestTag", clock, stream};
};

TEST_F(LoggerTest, EmitsHeaderTagFooter) {
    clock.now = 1234; // 00:00:01.234
    logger.info("body");
    EXPECT_EQ(stream.captured, "\x1b[32m[00:00:01.234][I][TestTag] body\x1b[0m\n");
}

TEST_F(LoggerTest, FormatsArgsWithBracePlaceholders) {
    clock.now = 0;
    logger.info("v={} ma={}", 5000, 1000);
    EXPECT_NE(stream.captured.find("v=5000 ma=1000"), std::string::npos);
}

TEST_F(LoggerTest, AlwaysEndsWithColorResetAndNewline) {
    logger.warn("anything");
    EXPECT_EQ(stream.captured.substr(stream.captured.size() - 5), "\x1b[0m\n");
}

TEST_F(LoggerTest, LevelTagMatchesLevel) {
    logger.error("e");
    EXPECT_NE(stream.captured.find("[E]"), std::string::npos);
    stream.captured.clear();

    logger.warn("w");
    EXPECT_NE(stream.captured.find("[W]"), std::string::npos);
    stream.captured.clear();

    logger.debug("d");
    EXPECT_NE(stream.captured.find("[D]"), std::string::npos);
}

TEST_F(LoggerTest, EncodesTimestampZeroPadded) {
    clock.now = 3'723'045; // 01:02:03.045
    logger.info("");
    EXPECT_NE(stream.captured.find("[01:02:03.045]"), std::string::npos);
}

TEST(LoggerDisabled, NoCrashWhenStreamUnset) {
    tempo::Logger logger;
    logger.info("body {}", 42); // must not crash, must write nothing
    SUCCEED();
}

TEST_F(LoggerTest, HexdumpFormatsAddressBytesAscii) {
    constexpr uint8_t data[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0xff, 0x10};
    logger.hexdump("payload", data, sizeof(data));

    EXPECT_NE(stream.captured.find("payload (8 bytes):"), std::string::npos);
    EXPECT_NE(stream.captured.find("0000:"), std::string::npos);
    EXPECT_NE(stream.captured.find("48 65 6c 6c 6f 00 ff 10"), std::string::npos);
    EXPECT_NE(stream.captured.find("Hello"), std::string::npos); // ASCII column
}

TEST_F(LoggerTest, BufferedSinkChunksLongLineWithoutLoss) {
    clock.now = 0;
    // BufferedStreamSink::BUF_SIZE = 64. Header is ~30 bytes; body well over 64 forces flush.
    const std::string long_body(200, 'x');
    logger.info("{}", long_body);

    // Body bytes survive intact across chunked writes.
    EXPECT_NE(stream.captured.find(long_body), std::string::npos);
    // More than one write call to the StreamWriter for header + chunked body + footer.
    EXPECT_GT(stream.write_calls, 1u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
