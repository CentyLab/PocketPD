#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <cstddef>
#include <cstdint>

#include "I2CDevice.h"

/**
 * @brief Concrete I2C device using Arduino TwoWire
 */
class ArduinoTwoWireDevice : public I2CDevice {
private:
    TwoWire& m_wire;
    const uint8_t m_address;

public:
    /**
     * Construct TwoWire-backed I2C device
     * @param wire    TwoWire bus instance
     * @param address 7-bit I2C slave address
     */
    ArduinoTwoWireDevice(TwoWire& wire, const uint8_t address) : m_wire(wire), m_address(address) {}

protected:
    bool read_bytes(uint8_t reg, uint8_t* const buf, size_t len) const override {
        m_wire.beginTransmission(m_address);
        m_wire.write(reg);
        if (m_wire.endTransmission() != 0) {
            return false;
        }

        if (m_wire.requestFrom(m_address, len) < len) {
            return false;
        }

        for (size_t i = 0; i < len && m_wire.available(); i++) {
            buf[i] = m_wire.read();
        }

        return true;
    }

    bool write_bytes(uint8_t reg, const uint8_t* const buf, size_t len) const override {
        m_wire.beginTransmission(m_address);
        m_wire.write(reg);
        for (size_t i = 0; i < len; i++) {
            m_wire.write(buf[i]);
        }

        return m_wire.endTransmission() == 0;
    }
};
