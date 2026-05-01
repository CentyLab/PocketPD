#pragma once
/**
 * @file I2CDevice.h
 * I2C device abstraction (header-only)
 *
 * Abstract interface for I2C register read/write.
 * Concrete implementations live in separate headers
 * (see ArduinoTwoWireDevice.h for an Arduino TwoWire backend).
 */

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * Abstract I2C device interface
 *
 * Provides register-level read/write contract.
 * Decouples drivers from specific I2C implementation.
 */
class I2CDevice {
public:
    virtual ~I2CDevice() = default;

    /**
     * Read len bytes from register into buf (non-virtual; calls do_read).
     * @return true on success, false on NACK or short read
     */
    bool read(uint8_t reg, uint8_t* const buf, size_t len) const {
        return read_bytes(reg, buf, len);
    }

    template <size_t N>
    constexpr bool read(uint8_t reg, std::array<uint8_t, N>& buf) const {
        return read(reg, buf.data(), buf.size());
    }

    /**
     * Write len bytes from buf to register (non-virtual; calls do_write).
     * @return true on success, false on NACK
     */
    bool write(uint8_t reg, const uint8_t* const buf, size_t len) const {
        return write_bytes(reg, buf, len);
    }

    template <size_t N>
    constexpr bool write(uint8_t reg, const std::array<uint8_t, N>& buf) const {
        return write(reg, buf.data(), buf.size());
    }

protected:
    virtual bool read_bytes(uint8_t reg, uint8_t* const buf, size_t len) const = 0;
    virtual bool write_bytes(uint8_t reg, const uint8_t* const buf, size_t len) const = 0;
};
