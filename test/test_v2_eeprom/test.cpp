#define VERSION "\"test\""

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "v2/hal/eeprom.h"

using namespace pocketpd;

TEST(EepromCodec, RoundTripDefaultsThenCustom) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};

    Preferences in_defaults{};
    encode_preferences(in_defaults, buf.data());

    Preferences out{};
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_EQ(out.restore_last_profile_enabled, false);

    Preferences in_custom{.restore_last_profile_enabled = true};
    encode_preferences(in_custom, buf.data());
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_EQ(out.restore_last_profile_enabled, true);
}

TEST(EepromCodec, VersionMismatchFailsDecode) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};
    Preferences in{.restore_last_profile_enabled = true};
    encode_preferences(in, buf.data());

    buf[0] = PREFERENCES_LAYOUT_VERSION + 1;

    Preferences out{};
    EXPECT_FALSE(decode_preferences(buf.data(), out));
}

TEST(EepromCodec, CrcCorruptionFailsDecode) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};
    Preferences in{.restore_last_profile_enabled = true};
    encode_preferences(in, buf.data());

    buf[1] ^= 0xFF;

    Preferences out{};
    EXPECT_FALSE(decode_preferences(buf.data(), out));
}

TEST(EepromCodec, VoltageCompFieldRoundTrips) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};

    Preferences in{
        .restore_last_profile_enabled = false,
        .voltage_compensate_enabled = true,
    };
    encode_preferences(in, buf.data());

    Preferences out{};
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_FALSE(out.restore_last_profile_enabled);
    EXPECT_TRUE(out.voltage_compensate_enabled);
}

TEST(EepromCodec, DefaultsHaveVoltageCompOff) {
    Preferences p{};
    EXPECT_FALSE(p.voltage_compensate_enabled);
}

TEST(EepromCodec, FlipDisplayFieldRoundTrips) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};

    Preferences in{
        .restore_last_profile_enabled = false,
        .voltage_compensate_enabled = false,
        .flip_display_enabled = true,
    };
    encode_preferences(in, buf.data());

    Preferences out{};
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_TRUE(out.flip_display_enabled);
}

TEST(EepromCodec, DefaultsHaveFlipDisplayOff) {
    Preferences p{};
    EXPECT_FALSE(p.flip_display_enabled);
}

TEST(EepromCodec, LayoutVersionIsFour) {
    EXPECT_EQ(PREFERENCES_LAYOUT_VERSION, 4);
}

TEST(EepromCodec, SavedProfileFieldsRoundTrip) {
    std::array<uint8_t, EEPROM_PREFERENCES_BYTES> buf{};

    Preferences in{
        .restore_last_profile_enabled = true,
        .last_profile = {
            .is_pps = true,
            .voltage_mv = 9000,
            .current_ma = 2500,
            .pdo_index = 3,
        },
    };
    encode_preferences(in, buf.data());

    Preferences out{};
    EXPECT_TRUE(decode_preferences(buf.data(), out));
    EXPECT_TRUE(out.last_profile.is_pps);
    EXPECT_EQ(out.last_profile.voltage_mv, 9000);
    EXPECT_EQ(out.last_profile.current_ma, 2500);
    EXPECT_EQ(out.last_profile.pdo_index, 3);
}

TEST(EepromCodec, SavedProfileDefaultsAreZero) {
    Preferences p{};
    EXPECT_FALSE(p.last_profile.is_pps);
    EXPECT_EQ(p.last_profile.voltage_mv, 0);
    EXPECT_EQ(p.last_profile.current_ma, 0);
    EXPECT_EQ(p.last_profile.pdo_index, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
