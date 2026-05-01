/**
 * @file pd_sink_controller.h
 * @brief Abstract USB-PD sink HAL.
 *
 * Stages, tasks, and tests depend only on this interface. The Arduino-backed
 * implementation lives in `ap33772_pd_sink.h`; mocks live in
 * `test/mocks/MockPdSink.h`.
 */
#pragma once

namespace pocketpd {

    /**
     * @brief Abstract USB-PD sink. Surface today covers negotiation and PDO
     * summary; later stages add set_pdo / set_pps_pdo / live readings.
     */
    class PdSinkController {
    public:
        virtual ~PdSinkController() = default;

        /**
         * @brief Negotiate with the source and load PDO list.
         * @return true if the controller reported READY+SUCCESS+NEWPDO and the
         * SRCPDO block was read without I2C error.
         */
        [[nodiscard]] virtual bool begin() = 0;

        /** @brief Number of valid source PDOs after a successful begin(). */
        virtual int pdo_count() const = 0;

        /** @brief Number of PPS APDOs in the PDO list (issue #24 multi-PPS). */
        virtual int pps_count() const = 0;

        /** @brief True if PDO at index is a PPS APDO. */
        virtual bool is_index_pps(int index) const = 0;

        /** @brief True if PDO at index is a fixed-supply PDO. */
        virtual bool is_index_fixed(int index) const = 0;

        /** @brief Max voltage in mV for a PDO; -1 if invalid index. */
        virtual int pdo_max_voltage_mv(int index) const = 0;

        /** @brief Min voltage in mV for a PDO (= max for fixed); -1 if invalid index. */
        virtual int pdo_min_voltage_mv(int index) const = 0;

        /** @brief Max current in mA for a PDO; -1 if invalid index. */
        virtual int pdo_max_current_ma(int index) const = 0;

        // —— Power requests

        /** @brief Request a fixed PDO by index. */
        [[nodiscard]] virtual bool set_pdo(int index) = 0;

        /** @brief Request a PPS APDO with explicit voltage and current. */
        [[nodiscard]] virtual bool set_pps_pdo(int index, int voltage_mv, int current_ma) = 0;
    };

} // namespace pocketpd
