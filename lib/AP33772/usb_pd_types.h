/**
 * @file usb_pd_types.h
 * USB PD 3.0 Power Delivery Object types and helper functions for use exclusively with AP33772 PD
 * Sink Controller. Note that this may not be accurate for other controllers.
 *
 * Struct layouts and scale factors from AP33772 Sink Controller EVB User Guide.
 * ref: https://www.diodes.com/assets/Evaluation-Boards/AP33772-Sink-Controller-EVB-User-Guide.pdf
 */
#ifndef USB_PD_TYPES_H
#define USB_PD_TYPES_H

#include <cstdint>

// ── PDO field scale factors
#define FIXED_PDO_VOLTAGE_LSB 50  // mV per LSB  B[19:10]
#define FIXED_PDO_CURRENT_LSB 10  // mA per LSB  B[9:0]
#define PPS_PDO_VOLTAGE_LSB   100 // mV per LSB  B[24:17], B[15:8]
#define PPS_PDO_CURRENT_LSB   50  // mA per LSB  B[6:0]

// ── RDO field scale factors
#define FIXED_RDO_CURRENT_LSB 10 // mA per LSB  B[19:10], B[9:0]
#define PPS_RDO_VOLTAGE_LSB   20 // mV per LSB  B[19:9]
#define PPS_RDO_CURRENT_LSB   50 // mA per LSB  B[6:0]

/**
 * Source Power Data Object
 *
 * Fixed Supply PDO:
 *   B[31:30] = 00  Fixed supply
 *   B[29:20]       Reserved
 *   B[19:10]       Voltage in 50 mV units
 *   B[9:0]         Maximum current in 10 mA units
 *
 * PPS APDO:
 *   B[31:30] = 11  Augmented PDO
 *   B[29:28] = 00  Programmable Power Supply
 *   B[27:25]       Reserved
 *   B[24:17]       Max voltage in 100 mV units
 *   B[16]          Reserved
 *   B[15:8]        Min voltage in 100 mV units
 *   B[7]           Reserved
 *   B[6:0]         Max current in 50 mA units
 */
struct pdo_t {

    // clang-format off
    union {
        struct {
            uint32_t max_current : 10;
            uint32_t voltage     : 10;
            uint32_t             : 10;
            uint32_t type        : 2;
        } fixed;
        struct {
            uint32_t max_current : 7;
            uint32_t             : 1;
            uint32_t min_voltage : 8;
            uint32_t             : 1;
            uint32_t max_voltage : 8;
            uint32_t             : 3;
            uint32_t apdo_type   : 2;
            uint32_t type        : 2;
        } pps;
        uint32_t raw;
    };
    // clang-format on

    // —— Spec type codes (B[31:30]) and APDO sub-type (B[29:28])
    static constexpr uint32_t TYPE_FIXED = 0b00;
    static constexpr uint32_t TYPE_AUGMENTED = 0b11;
    static constexpr uint32_t APDO_PPS = 0b00;

    enum pdo_type_t : uint8_t { FIXED, PPS, UNKNOWN };

    pdo_type_t pdo_type() const {
        switch (fixed.type) {
        case TYPE_FIXED:
            return FIXED;
        case TYPE_AUGMENTED:
            return pps.apdo_type == APDO_PPS ? PPS : UNKNOWN;
        default:
            return UNKNOWN;
        }
    }

    bool is_fixed() const {
        return pdo_type() == FIXED;
    }
    bool is_pps() const {
        return pdo_type() == PPS;
    }

    int max_voltage_mv() const {
        switch (pdo_type()) {
        case FIXED:
            return fixed.voltage * FIXED_PDO_VOLTAGE_LSB;
        case PPS:
            return pps.max_voltage * PPS_PDO_VOLTAGE_LSB;
        default:
            return -1;
        }
    }

    int min_voltage_mv() const {
        switch (pdo_type()) {
        case FIXED:
            return fixed.voltage * FIXED_PDO_VOLTAGE_LSB;
        case PPS:
            return pps.min_voltage * PPS_PDO_VOLTAGE_LSB;
        default:
            return -1;
        }
    }

    int max_current_ma() const {
        switch (pdo_type()) {
        case FIXED:
            return fixed.max_current * FIXED_PDO_CURRENT_LSB;
        case PPS:
            return pps.max_current * PPS_PDO_CURRENT_LSB;
        default:
            return -1;
        }
    }
};

static_assert(sizeof(pdo_t) == 4, "pdo_t must be 4 bytes");

/**
 * Request Data Object
 * ```
 * Fixed RDO:
 *   B[9:0]         Max operating current in 10 mA units
 *   B[19:10]       Operating current in 10 mA units
 *   B[27:20]       Reserved (0)
 *   B[30:28]       Object position (1-based, 000 reserved)
 *   B[31]          Reserved (0)
 *
 * Programmable RDO:
 *   B[6:0]         Operating current in 50 mA units
 *   B[8:7]         Reserved (0)
 *   B[19:9]        Output voltage in 20 mV units
 *   B[27:20]       Reserved (0)
 *   B[30:28]       Object position (1-based, 000 reserved)
 *   B[31]          Reserved (0)
 * ```
 */
struct rdo_t {
    union {
        struct {
            uint32_t max_current : 10;
            uint32_t op_current : 10;
            uint32_t : 8;
            uint32_t obj_position : 3;
            uint32_t : 1;
        } fixed;
        struct {
            uint32_t op_current : 7;
            uint32_t : 2;
            uint32_t voltage : 11;
            uint32_t : 8;
            uint32_t obj_position : 3;
            uint32_t : 1;
        } pps;
        uint32_t raw;
    };
};

static_assert(sizeof(rdo_t::raw) == 4, "rdo_t size is not 4 bytes");

#endif // USB_PD_TYPES_H
