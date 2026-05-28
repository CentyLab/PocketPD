#define VERSION "\"test\""

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "v2/hal/eeprom.h"

using namespace pocketpd;

TEST(EepromCodec, RoundTripDefaultsThenCustom) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};

    Preferences in_defaults{};
    encode_preferences(in_defaults, buf.data());

    Preferences out{};
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_EQ(out.skip_picker_on_boot, false);

    Preferences in_custom{ .skip_picker_on_boot = true };
    encode_preferences(in_custom, buf.data());
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_EQ(out.skip_picker_on_boot, true);
}

TEST(EepromCodec, VersionMismatchFailsDecode) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};
    Preferences in{ .skip_picker_on_boot = true };
    encode_preferences(in, buf.data());

    buf[0] = PREFERENCES_LAYOUT_VERSION + 1;

    Preferences out{};
    EXPECT_FALSE(decode_preferences(buf.data(), out));
}

TEST(EepromCodec, CrcCorruptionFailsDecode) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};
    Preferences in{ .skip_picker_on_boot = true };
    encode_preferences(in, buf.data());

    buf[1] ^= 0xFF;

    Preferences out{};
    EXPECT_FALSE(decode_preferences(buf.data(), out));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
