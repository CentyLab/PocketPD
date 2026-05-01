#ifndef MOCK_I2C_DEVICE_H
#define MOCK_I2C_DEVICE_H

#include <gmock/gmock.h>

#include "I2CDevice.h"

class MockI2CDevice : public I2CDevice {
public:
    MOCK_METHOD(bool, read_bytes, (uint8_t reg, uint8_t* buf, size_t len), (const, override));
    MOCK_METHOD(bool, write_bytes, (uint8_t reg, const uint8_t* buf, size_t len), (const, override));
};

#endif // MOCK_I2C_DEVICE_H
