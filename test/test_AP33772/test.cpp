/**
 * GoogleTest/GoogleMock suite for AP33772 driver.
 *
 * Runs on host (native env): pio test -e native
 *
 * Uses GMock MockI2CDevice (scripted reads + EXPECT_CALL assertions on writes)
 * and a no-op delay function (no real sleep).
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include <MockI2CDevice.h>
#include <AP33772.h>

using namespace ap33772;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArrayArgument;

/**
 * @brief 5V @ 3A Fixed PDO.
 *
 * Bit layout:
 * ```
 *  00      0000000000  0001100100   0100101100
 *  ^^      ^^^^^^^^^^  ^^^^^^^^^^   ^^^^^^^^^^
 *  type    reserved    voltage      max_current
 *  [31:30] [29:20]     [19:10]      [9:0]
 *  Fixed               100×50mV=5V  300×10mA=3A
 * ```
 */
static std::vector<uint8_t> fixed_pdo_5v_3a() {
    const uint32_t raw = (0u << 30) | (100u << 10) | 300u;
    return {
        static_cast<uint8_t>(raw & 0xFF),
        static_cast<uint8_t>((raw >> 8) & 0xFF),
        static_cast<uint8_t>((raw >> 16) & 0xFF),
        static_cast<uint8_t>((raw >> 24) & 0xFF),
    };
}

/**
 * @brief 3.3V–11V @ 3A PPS APDO.
 *
 * Bit layout:
 * ```
 *  11      00       000      01101110       0    00100001       0    0111100
 *  ^^      ^^       ^^^      ^^^^^^^^       ^    ^^^^^^^^       ^    ^^^^^^^
 *  type    apdo     rsv      max_voltage    rsv  min_voltage    rsv  max_current
 *  [31:30] [29:28]  [27:25]  [24:17]        [16] [15:8]         [7]  [6:0]
 *  APDO    PPS               110×100mV=11V       33×100mV=3.3V       60×50mA=3A
 * ```
 */
static std::vector<uint8_t> pps_pdo_3v3_11v_3a() {
    const uint32_t raw = (3u << 30) | (0u << 28) | (110u << 17) | (33u << 8) | 60u;
    return {
        static_cast<uint8_t>(raw & 0xFF),
        static_cast<uint8_t>((raw >> 8) & 0xFF),
        static_cast<uint8_t>((raw >> 16) & 0xFF),
        static_cast<uint8_t>((raw >> 24) & 0xFF),
    };
}

static std::vector<uint8_t> pack_srcpdo(std::vector<std::vector<uint8_t>> pdos) {
    std::vector<uint8_t> out(28, 0);
    size_t offset = 0;
    for (const auto& p : pdos) {
        for (auto b : p) {
            if (offset >= out.size()) {
                break;
            }
            out[offset++] = b;
        }
    }
    return out;
}

// STATUS = ready + success + newpdo (bits 0,1,2 = 0x07).
static constexpr uint8_t STATUS_OK = 0x07;
static void noop_delay(unsigned long) {}

class AP33772Test : public ::testing::Test {
protected:
    NiceMock<MockI2CDevice> m_i2c_device;

    // Buffer storage for GMock SetArrayArgument (lifetimes must outlive expectations).
    uint8_t m_status_byte = 0;
    uint8_t m_count_byte = 0;
    std::vector<uint8_t> m_srcpdo_buf;

    // Script the standard "healthy PDO list" read sequence.
    void scriptHealthyBus(std::vector<std::vector<uint8_t>> pdos) {
        m_status_byte = STATUS_OK;
        m_count_byte = static_cast<uint8_t>(pdos.size());
        m_srcpdo_buf = pack_srcpdo(pdos);

        ON_CALL(m_i2c_device, read_bytes(reg::STATUS, _, 1))
            .WillByDefault(
                DoAll(SetArrayArgument<1>(&m_status_byte, &m_status_byte + 1), Return(true))
            );
        ON_CALL(m_i2c_device, read_bytes(reg::PDONUM, _, 1))
            .WillByDefault(
                DoAll(SetArrayArgument<1>(&m_count_byte, &m_count_byte + 1), Return(true))
            );
        ON_CALL(m_i2c_device, read_bytes(reg::SRCPDO, _, 28))
            .WillByDefault(DoAll(
                SetArrayArgument<1>(m_srcpdo_buf.data(), m_srcpdo_buf.data() + 28), Return(true)
            ));
    }

    // Script a single-byte read response (status, count, etc).
    void scriptReadByte(uint8_t reg_addr, uint8_t value, uint8_t* storage) {
        *storage = value;
        ON_CALL(m_i2c_device, read_bytes(reg_addr, _, 1))
            .WillByDefault(DoAll(SetArrayArgument<1>(storage, storage + 1), Return(true)));
    }
};

// —— begin() behavior
TEST_F(AP33772Test, BeginFailsWhenOVPActive) {
    scriptReadByte(reg::STATUS, 0x10, &m_status_byte);
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_FALSE(ap.begin());
}

TEST_F(AP33772Test, BeginFailsWhenNotReady) {
    scriptReadByte(reg::STATUS, 0x00, &m_status_byte);
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_FALSE(ap.begin());
}

TEST_F(AP33772Test, BeginLoadsPDOsOnSuccess) {
    scriptHealthyBus({fixed_pdo_5v_3a(), pps_pdo_3v3_11v_3a()});
    AP33772 ap(m_i2c_device, noop_delay);

    ASSERT_TRUE(ap.begin());
    EXPECT_EQ(2, ap.get_count_pdo());
    EXPECT_EQ(1, ap.get_count_pps());
    EXPECT_TRUE(ap.has_pps_profile());
}

// —— PDO decode
TEST_F(AP33772Test, FixedPDODecode) {
    scriptHealthyBus({fixed_pdo_5v_3a()});
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());

    EXPECT_EQ(5000, ap.get_pdo_max_voltage(0));
    EXPECT_EQ(5000, ap.get_pdo_min_voltage(0));
    EXPECT_EQ(3000, ap.get_pdo_max_current(0));
    EXPECT_FALSE(ap.is_index_pps(0));
}

TEST_F(AP33772Test, PPSPDODecode) {
    scriptHealthyBus({pps_pdo_3v3_11v_3a()});
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());

    EXPECT_TRUE(ap.is_index_pps(0));
    EXPECT_EQ(3300, ap.get_pdo_min_voltage(0));
    EXPECT_EQ(11000, ap.get_pdo_max_voltage(0));
    EXPECT_EQ(3000, ap.get_pdo_max_current(0));
}

// —— Bounds
TEST_F(AP33772Test, OutOfRangeIndexReturnsMinusOne) {
    scriptHealthyBus({fixed_pdo_5v_3a()});
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());

    EXPECT_EQ(-1, ap.get_pdo_max_voltage(99));
    EXPECT_EQ(-1, ap.get_pdo_max_voltage(-1));
    EXPECT_FALSE(ap.is_index_pps(-1));
    EXPECT_FALSE(ap.is_index_pps(99));
}

// —— Request paths
TEST_F(AP33772Test, SetPPSPDORejectsNonPPSIndex) {
    scriptHealthyBus({fixed_pdo_5v_3a()});

    // RDO write must NOT be called.
    EXPECT_CALL(m_i2c_device, write_bytes(reg::RDO, _, _)).Times(0);

    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());
    EXPECT_FALSE(ap.set_pps_pdo(0, 5000, 1000));
}

TEST_F(AP33772Test, SetPDOWritesRDO) {
    scriptHealthyBus({fixed_pdo_5v_3a()});

    EXPECT_CALL(m_i2c_device, write_bytes(reg::RDO, _, 4)).WillOnce(Return(true));

    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());
    EXPECT_TRUE(ap.set_pdo(0));
}

TEST_F(AP33772Test, FindPPSMatch) {
    scriptHealthyBus({fixed_pdo_5v_3a(), pps_pdo_3v3_11v_3a()});
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());

    EXPECT_EQ(1, ap.find_pps_index_by_voltage_current(5000, 1000));
    EXPECT_EQ(-1, ap.find_pps_index_by_voltage_current(20000, 1000)); // out of range
    EXPECT_EQ(-1, ap.find_pps_index_by_voltage_current(5000, 5000));  // too much current
}

TEST_F(AP33772Test, SetPDORejectsPPSIndex) {
    scriptHealthyBus({fixed_pdo_5v_3a(), pps_pdo_3v3_11v_3a()});

    EXPECT_CALL(m_i2c_device, write_bytes(reg::RDO, _, _)).Times(0);

    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());
    EXPECT_FALSE(ap.set_pdo(1)); // PPS index — must reject
}

TEST_F(AP33772Test, FailedPPSRequestDoesNotMutateActiveState) {
    scriptHealthyBus({fixed_pdo_5v_3a(), pps_pdo_3v3_11v_3a()});

    // First write: fixed RDO succeeds (set_pdo).
    // Second write: PPS RDO fails. State must stay on PDO 0 / fixed.
    // Third write: set_current rebuilds for active=PDO 0 (fixed RDO).
    EXPECT_CALL(m_i2c_device, write_bytes(reg::RDO, _, 4))
        .WillOnce(Return(true))   // set_pdo(0)
        .WillOnce(Return(false))  // set_pps_pdo(1, ...) fails
        .WillOnce(Return(true));  // set_current(...) — must use fixed path

    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.begin());
    EXPECT_TRUE(ap.set_pdo(0));
    EXPECT_EQ(0, ap.get_active_pdo());

    EXPECT_FALSE(ap.set_pps_pdo(1, 5000, 1000));
    EXPECT_EQ(0, ap.get_active_pdo()); // unchanged on failure

    EXPECT_TRUE(ap.set_current(1500));
}

// —— Protection thresholds
TEST_F(AP33772Test, SetOcpThresholdAcceptsBoundary) {
    EXPECT_CALL(m_i2c_device, write_bytes(reg::OCPTHR, _, 1)).WillOnce(Return(true));
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.set_ocp_threshold(255 * OCP_LSB)); // 12750 mA — max
}

TEST_F(AP33772Test, SetOcpThresholdRejectsOutOfRange) {
    EXPECT_CALL(m_i2c_device, write_bytes(reg::OCPTHR, _, _)).Times(0);
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_FALSE(ap.set_ocp_threshold(-1));
    EXPECT_FALSE(ap.set_ocp_threshold(255 * OCP_LSB + 1));
}

TEST_F(AP33772Test, SetOtpThresholdRejectsOutOfRange) {
    EXPECT_CALL(m_i2c_device, write_bytes(reg::OTPTHR, _, _)).Times(0);
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_FALSE(ap.set_otp_threshold(-1));
    EXPECT_FALSE(ap.set_otp_threshold(256));
}

TEST_F(AP33772Test, SetDeratingThresholdRejectsOutOfRange) {
    EXPECT_CALL(m_i2c_device, write_bytes(reg::DRTHR, _, _)).Times(0);
    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_FALSE(ap.set_derating_threshold(-1));
    EXPECT_FALSE(ap.set_derating_threshold(256));
}

// —— reset
TEST_F(AP33772Test, ResetWritesZeroRDO) {
    std::vector<uint8_t> captured;
    EXPECT_CALL(m_i2c_device, write_bytes(reg::RDO, _, 4))
        .WillOnce(DoAll(
            Invoke([&](uint8_t, const uint8_t* p, size_t n) { captured.assign(p, p + n); }),
            Return(true)
        ));

    AP33772 ap(m_i2c_device, noop_delay);
    EXPECT_TRUE(ap.reset());

    EXPECT_THAT(captured, ElementsAre(0, 0, 0, 0));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
