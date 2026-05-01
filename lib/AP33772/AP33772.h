/**
 * @file AP33772.h
 * AP33772 USB PD 3.0 Sink Controller Driver
 *
 * Reference: AP33772 Sink Controller EVB User Guide
 * https://www.diodes.com/assets/Evaluation-Boards/AP33772-Sink-Controller-EVB-User-Guide.pdf
 *
 * Register map and bit definitions derived from official documentation.
 */
#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "I2CDevice.h"
#include "usb_pd_types.h"

namespace ap33772 {

    template <typename T>
    constexpr T clamp(T v, T lo, T hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // ── AP33772 constants
    constexpr uint8_t ADDRESS = 0x51;
    constexpr uint8_t MAX_PDO = 7;
    constexpr uint8_t SRCPDO_LENGTH = 28; // 7 PDOs x 4 bytes each

    // ── Register addresses (Table 4 – EVB User Guide)
    // clang-format off
    namespace reg {
        constexpr uint8_t SRCPDO  = 0x00; // RO   28 bytes  Source PDOs (7 x 4 bytes)
        constexpr uint8_t PDONUM  = 0x1C; // RO    1 byte   Number of valid source PDOs
        constexpr uint8_t STATUS  = 0x1D; // RC    1 byte   Status (read-clear)
        constexpr uint8_t MASK    = 0x1E; // RW    1 byte   Interrupt enable mask
        constexpr uint8_t VOLTAGE = 0x20; // RO    1 byte   VBUS voltage   (LSB = 80 mV)
        constexpr uint8_t CURRENT = 0x21; // RO    1 byte   VBUS current   (LSB = 24 mA)
        constexpr uint8_t TEMP    = 0x22; // RO    1 byte   NTC temperature (LSB = 1 °C)
        constexpr uint8_t OCPTHR  = 0x23; // RW    1 byte   OCP threshold (LSB = 50 mA)
        constexpr uint8_t OTPTHR  = 0x24; // RW    1 byte   OTP threshold (LSB = 1 °C)
        constexpr uint8_t DRTHR   = 0x25; // RW    1 byte   Derating threshold (LSB = 1 °C)
        constexpr uint8_t TR25    = 0x28; // RW    2 bytes  NTC resistance @ 25 °C  (Ω)
        constexpr uint8_t TR50    = 0x2A; // RW    2 bytes  NTC resistance @ 50 °C  (Ω)
        constexpr uint8_t TR75    = 0x2C; // RW    2 bytes  NTC resistance @ 75 °C  (Ω)
        constexpr uint8_t TR100   = 0x2E; // RW    2 bytes  NTC resistance @ 100 °C (Ω)
        constexpr uint8_t RDO     = 0x30; // WO    4 bytes  Request Data Object
        constexpr uint8_t VID     = 0x34; // RW    2 bytes  Vendor ID (reserved)
        constexpr uint8_t PID     = 0x36; // RW    2 bytes  Product ID (reserved)
    } // namespace reg
    // clang-format on

    // ── Register LSB scale factors
    constexpr int VOLTAGE_LSB = 80; // mV per LSB
    constexpr int CURRENT_LSB = 24; // mA per LSB
    constexpr int OCP_LSB = 50;     // mA per LSB

    // ── NTC defaults (Table 4)
    constexpr int NTC_DEFAULT_TR25 = 10000; // 0x2710
    constexpr int NTC_DEFAULT_TR50 = 4161;  // 0x1041
    constexpr int NTC_DEFAULT_TR75 = 1928;  // 0x0788
    constexpr int NTC_DEFAULT_TR100 = 974;  // 0x03CE

    /**
     * STATUS register (0x1D) — read-clear (Table 7)
     */
    struct status_reg_t {
        union {
            struct {
                uint8_t ready : 1;   // B0 – bits[2:1] valid when set
                uint8_t success : 1; // B1 – negotiation result
                uint8_t newpdo : 1;  // B2 – source PDOs received
                uint8_t : 1;         // B3 – reserved
                uint8_t ovp : 1;     // B4 – over-voltage protection
                uint8_t ocp : 1;     // B5 – over-current protection
                uint8_t otp : 1;     // B6 – over-temp protection
                uint8_t dr : 1;      // B7 – derating started
            };
            uint8_t raw;
        };
    };

    /**
     * MASK register (0x1E) — interrupt enable (Table 9)
     *
     * Mirrors STATUS layout. Set bit to 1 to enable interrupt for that event.
     * Default value: 0x01 (only READY enabled).
     */
    struct mask_reg_t {
        union {
            struct {
                uint8_t ready_en : 1;   // B0 – default 1
                uint8_t success_en : 1; // B1
                uint8_t newpdo_en : 1;  // B2
                uint8_t : 1;            // B3 – reserved
                uint8_t ovp_en : 1;     // B4
                uint8_t ocp_en : 1;     // B5
                uint8_t otp_en : 1;     // B6
                uint8_t dr_en : 1;      // B7
            };
            uint8_t raw;
        };
    };

    using delay_fn_t = void (*)(unsigned long);

    /**
     * AP33772 USB PD 3.0 Sink Controller driver
     *
     * Supports multiple PPS profiles, fixed PDOs, voltage/current requests,
     * protection thresholds, and NTC temperature sensing.
     */
    class AP33772 {
    private:
        const I2CDevice& m_i2c;
        const delay_fn_t m_delay;

        int m_pdo_active = 0;
        int m_pdo_count = 0;
        std::array<pdo_t, MAX_PDO> m_pdo_list{0};

        int m_pps_count = 0;
        int m_pps_voltage_20mv = 0; // Last PPS voltage in 20 mV units
        uint8_t m_pps_mask = 0;     // Bitmask: bit N set if PDO[N] is PPS

        // —— RDO helpers

        bool write_rdo(const rdo_t& rdo) const {
            std::array<uint8_t, 4> buffer{0};
            memcpy(buffer.data(), &rdo.raw, buffer.size());
            return m_i2c.write(reg::RDO, buffer.data(), buffer.size());
        }

        bool request_pps(int pdo_index, int voltage_mv, int current_ma) {
            const int voltage_20mv = voltage_mv / PPS_RDO_VOLTAGE_LSB;

            const rdo_t rdo = {
                .pps = {
                    .op_current = static_cast<uint32_t>(current_ma / PPS_RDO_CURRENT_LSB),
                    .voltage = static_cast<uint32_t>(voltage_20mv),
                    .obj_position = static_cast<uint32_t>(pdo_index + 1),
                }
            };

            const bool success = write_rdo(rdo);
            if (success) {
                m_pdo_active = pdo_index;
                m_pps_voltage_20mv = voltage_20mv;
            }

            return success;
        }

        bool request_fixed(int pdo_index, int current_ma) {
            const rdo_t rdo = {
                .fixed = {
                    .max_current = static_cast<uint32_t>(current_ma / FIXED_RDO_CURRENT_LSB),
                    .op_current = static_cast<uint32_t>(current_ma / FIXED_RDO_CURRENT_LSB),
                    .obj_position = pdo_index + 1u,
                }
            };

            const bool success = write_rdo(rdo);
            if (success) {
                m_pdo_active = pdo_index;
            }

            return success;
        }

        /** Write a 16-bit little-endian NTC register */
        bool write_ntc_register(uint8_t reg, int value) const {
            std::array<uint8_t, 2> buffer{
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
            };

            return m_i2c.write(reg, buffer.data(), buffer.size());
        }

        int clamp_voltage(int index, int voltage_mv) const {
            int min_voltage = m_pdo_list.at(index).min_voltage_mv();
            int max_voltage = m_pdo_list.at(index).max_voltage_mv();
            return clamp(voltage_mv, min_voltage, max_voltage);
        }

        int clamp_current(int index, int current_ma) const {
            int max_current = m_pdo_list.at(index).max_current_ma();
            return clamp(current_ma, 0, max_current);
        }

    public:
        /**
         * Construct a new AP33772 driver
         * @param i2c   I2C device interface (e.g. TwoWireDevice)
         * @param delay Delay function (ms). Use Arduino `delay` in firmware, no-op in tests.
         */
        AP33772(const I2CDevice& i2c, const delay_fn_t delay) : m_i2c(i2c), m_delay(delay) {}

        int get_count_pdo() const {
            return m_pdo_count;
        }

        int get_count_pps() const {
            return m_pps_count;
        }

        int get_active_pdo() const {
            return m_pdo_active;
        }

        bool has_pps_profile() const {
            return m_pps_mask != 0;
        }

        /**
         * Initialize the controller and load source PDOs from charger
         * @return true if negotiation succeeded and PDOs were loaded
         */
        [[nodiscard]]
        bool begin() {
            const status_reg_t status = read_status();
            if (status.ovp || status.ocp || !status.ready || !status.success || !status.newpdo) {
                return false;
            }

            m_delay(10);

            uint8_t count = 0;
            if (!m_i2c.read(reg::PDONUM, &count, 1)) {
                return false;
            }

            m_pdo_count = clamp<int>(count, 0, MAX_PDO);

            std::array<uint8_t, SRCPDO_LENGTH> pdo_buf{0};
            if (!m_i2c.read(reg::SRCPDO, pdo_buf.data(), pdo_buf.size())) {
                return false;
            }

            m_pps_mask = 0;
            m_pps_count = 0;

            for (int i = 0; i < m_pdo_count; i++) {
                memcpy(&m_pdo_list.at(i).raw, &pdo_buf.at(i * 4), 4);

                if (m_pdo_list.at(i).is_pps()) {
                    m_pps_mask |= (1 << i);
                    m_pps_count++;
                }
            }

            return true;
        }

        // ── Power requests

        /**
         * Select a fixed PDO by index
         * @param pdo_index zero-based PDO index (must be a fixed PDO)
         * @return true if request was sent, false on invalid index or I2C error
         */
        [[nodiscard]]
        bool set_pdo(int pdo_index) {
            if (!is_index_fixed(pdo_index)) {
                return false;
            }

            return request_fixed(pdo_index, m_pdo_list.at(pdo_index).max_current_ma());
        }

        /**
         * Request a specific PPS profile with voltage and current
         * @param pdo_index zero-based PDO index (must be a PPS APDO)
         * @param voltage_mv target voltage in millivolts (clamped to range)
         * @param current_ma current limit in milliamps (clamped to max)
         * @return true if request was sent, false on invalid index or I2C error
         */
        [[nodiscard]]
        bool set_pps_pdo(int pdo_index, int voltage_mv, int current_ma) {
            if (!is_index_pps(pdo_index)) {
                return false;
            }

            voltage_mv = clamp_voltage(pdo_index, voltage_mv);
            current_ma = clamp_current(pdo_index, current_ma);

            return request_pps(pdo_index, voltage_mv, current_ma);
        }

        /**
         * Adjust voltage on the active PPS profile
         *
         * Only works if the active profile is PPS. Uses max current.
         * For specific current, use set_pps_pdo() or call set_current() after.
         *
         * @param voltage_mv target voltage in millivolts (clamped to range)
         * @return true if request was sent, false if active profile is fixed or I2C error
         */
        [[nodiscard]]
        bool set_voltage(int voltage_mv) {
            if (!is_index_pps(m_pdo_active)) {
                return false;
            }

            int voltage = clamp_voltage(m_pdo_active, voltage_mv);
            int max_current = get_pdo_max_current(m_pdo_active);
            return request_pps(m_pdo_active, voltage, max_current);
        }

        /**
         * Adjust current on the active profile (PPS or fixed)
         * @param current_ma current limit in milliamps (clamped to max)
         * @return true if request was sent, false on I2C error
         */
        [[nodiscard]]
        bool set_current(int current_ma) {

            int max_current = get_pdo_max_current(m_pdo_active);
            int clamped_ma = clamp(current_ma, 0, max_current);

            if (is_index_pps(m_pdo_active)) {
                return request_pps(
                    m_pdo_active, m_pps_voltage_20mv * PPS_RDO_VOLTAGE_LSB, clamped_ma
                );
            }

            return request_fixed(m_pdo_active, clamped_ma);
        }

        // ── Live readings

        /**
         * Read VBUS voltage from the AP33772
         * @return voltage in millivolts (LSB = 80 mV), -1 on I2C error
         */
        int read_voltage() const {
            uint8_t buf = 0;
            if (!m_i2c.read(reg::VOLTAGE, &buf, 1)) {
                return -1;
            }
            return buf * VOLTAGE_LSB;
        }

        /**
         * Read VBUS current from the AP33772
         * @return current in milliamps (LSB = 24 mA), -1 on I2C error
         */
        int read_current() const {
            uint8_t buf = 0;
            if (!m_i2c.read(reg::CURRENT, &buf, 1)) {
                return -1;
            }
            return buf * CURRENT_LSB;
        }

        /**
         * Read NTC temperature from the AP33772
         * @return temperature in degrees Celsius, -1 on I2C error
         */
        int read_temp() const {
            uint8_t buf = 0;
            if (!m_i2c.read(reg::TEMP, &buf, 1)) {
                return -1;
            }
            return buf;
        }

        /**
         * Read the STATUS register (read-clear)
         * @return status_reg_t with named bit fields
         */
        status_reg_t read_status() const {
            status_reg_t status = {0};
            m_i2c.read(reg::STATUS, &status.raw, 1);
            return status;
        }

        /**
         * Find a PPS profile that can deliver the requested V/I
         * @param voltage_mv target voltage in millivolts
         * @param current_ma target current in milliamps
         * @return zero-based PDO index, or -1 if no suitable PPS found
         */
        int find_pps_index_by_voltage_current(int voltage_mv, int current_ma) const {
            for (int i = 0; i < m_pdo_count; i++) {
                if (!is_index_pps(i)) {
                    continue;
                }

                if (voltage_mv >= m_pdo_list.at(i).min_voltage_mv() &&
                    voltage_mv <= m_pdo_list.at(i).max_voltage_mv() &&
                    current_ma <= m_pdo_list.at(i).max_current_ma()) {
                    return i;
                }
            }

            return -1;
        }

        /**
         * Check if a PDO index is valid
         */
        bool is_index_valid(int index) const {
            return index >= 0 && index < m_pdo_count;
        }

        /**
         * Check if a PDO index is a PPS APDO
         */
        bool is_index_pps(int index) const {
            return is_index_valid(index) && ((m_pps_mask >> index) & 1);
        }

        /**
         * Check if a PDO index is a fixed-supply PDO
         */
        bool is_index_fixed(int index) const {
            return is_index_valid(index) && m_pdo_list.at(index).is_fixed();
        }

        /**
         * Get PDO voltage (fixed: nominal, PPS: max)
         * @param index zero-based PDO index
         * @return voltage in millivolts, -1 if invalid index
         */
        int get_pdo_max_voltage(int index) const {
            return is_index_valid(index) ? m_pdo_list.at(index).max_voltage_mv() : -1;
        }

        /**
         * Get PDO minimum voltage (fixed: same as nominal, PPS: min)
         * @param index zero-based PDO index
         * @return voltage in millivolts, -1 if invalid index
         */
        int get_pdo_min_voltage(int index) const {
            return is_index_valid(index) ? m_pdo_list.at(index).min_voltage_mv() : -1;
        }

        /**
         * Get PDO maximum current
         * @param index zero-based PDO index
         * @return current in milliamps, -1 if invalid index
         */
        int get_pdo_max_current(int index) const {
            return is_index_valid(index) ? m_pdo_list.at(index).max_current_ma() : -1;
        }

        // ── Protection thresholds

        /**
         * Set over-current protection threshold
         * @param current_ma threshold in milliamps (LSB = 50 mA, range 0..12750)
         * @return false if out of range or I2C error
         */
        [[nodiscard]]
        bool set_ocp_threshold(int current_ma) const {
            if (current_ma < 0 || current_ma > 255 * OCP_LSB) {
                return false;
            }
            const uint8_t val = current_ma / OCP_LSB;
            return m_i2c.write(reg::OCPTHR, &val, 1);
        }

        /**
         * Set over-temperature protection threshold
         * @param temp_c threshold in degrees Celsius (range 0..255, default 120 °C)
         * @return false if out of range or I2C error
         */
        [[nodiscard]]
        bool set_otp_threshold(int temp_c) const {
            if (temp_c < 0 || temp_c > 255) {
                return false;
            }
            const uint8_t val = temp_c;
            return m_i2c.write(reg::OTPTHR, &val, 1);
        }

        /**
         * Set thermal derating threshold
         * @param temp_c threshold in degrees Celsius (range 0..255, default 120 °C)
         * @return false if out of range or I2C error
         */
        [[nodiscard]]
        bool set_derating_threshold(int temp_c) const {
            if (temp_c < 0 || temp_c > 255) {
                return false;
            }
            const uint8_t val = temp_c;
            return m_i2c.write(reg::DRTHR, &val, 1);
        }

        // ── NTC configuration

        /**
         * Set NTC thermistor resistance at four temperature points
         * @param tr25  resistance at 25 °C in ohms  (default 10000)
         * @param tr50  resistance at 50 °C in ohms  (default 4161)
         * @param tr75  resistance at 75 °C in ohms  (default 1928)
         * @param tr100 resistance at 100 °C in ohms (default 974)
         */
        [[nodiscard]]
        bool set_ntc(int tr25, int tr50, int tr75, int tr100) const {
            bool success = true;
            success &= write_ntc_register(reg::TR25, tr25);
            m_delay(5);
            success &= write_ntc_register(reg::TR50, tr50);
            m_delay(5);
            success &= write_ntc_register(reg::TR75, tr75);
            m_delay(5);
            success &= write_ntc_register(reg::TR100, tr100);
            return success;
        }

        // ── Interrupt mask

        /**
         * Read the current interrupt mask register
         * @return mask_reg_t with named bit fields
         */
        mask_reg_t read_mask() const {
            mask_reg_t mask = {0};
            m_i2c.read(reg::MASK, &mask.raw, 1);
            return mask;
        }

        /**
         * Write the interrupt mask register
         * @param mask mask_reg_t with desired bits set
         */
        [[nodiscard]]
        bool write_mask(const mask_reg_t mask) const {
            return m_i2c.write(reg::MASK, &mask.raw, 1);
        }

        // ── Utility

        /**
         * Hard reset the AP33772 and PD source
         *
         * Sends all-zero RDO which triggers a hard reset. Both AP33772 and
         * PD source return to default states. Output NMOS is turned off.
         */
        [[nodiscard]]
        bool reset() const {
            std::array<uint8_t, 4> buffer{0};
            return m_i2c.write(reg::RDO, buffer.data(), buffer.size());
        }

    };

} // namespace ap33772
