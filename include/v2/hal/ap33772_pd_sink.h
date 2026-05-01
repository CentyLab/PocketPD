/**
 * @file ap33772_pd_sink.h
 * @brief AP33772-backed implementation of `PdSinkController`.
 *
 * Owns an `ap33772::AP33772` driver instance and forwards `PdSinkController`
 * calls to it. Borrows the I2C bus through an `I2CDevice` reference.
 */
#pragma once

#include <AP33772.h>
#include <I2CDevice.h>

#include "v2/hal/pd_sink_controller.h"

namespace pocketpd {

    class Ap33772PdSink : public PdSinkController {
    private:
        ap33772::AP33772 m_driver;

    public:
        Ap33772PdSink(const I2CDevice& i2c, ap33772::delay_fn_t delay) : m_driver(i2c, delay) {}

        bool begin() override {
            return m_driver.begin();
        }
        int pdo_count() const override {
            return m_driver.pdo_count();
        }
        int pps_count() const override {
            return m_driver.pps_count();
        }
        bool is_index_pps(int index) const override {
            return m_driver.is_index_pps(index);
        }
        bool is_index_fixed(int index) const override {
            return m_driver.is_index_fixed(index);
        }
        int pdo_max_voltage_mv(int index) const override {
            return m_driver.pdo_max_voltage_mv(index);
        }
        int pdo_min_voltage_mv(int index) const override {
            return m_driver.pdo_min_voltage_mv(index);
        }
        int pdo_max_current_ma(int index) const override {
            return m_driver.pdo_max_current_ma(index);
        }
        bool set_pdo(int index) override {
            return m_driver.set_pdo(index);
        }
        bool set_pps_pdo(int index, int voltage_mv, int current_ma) override {
            return m_driver.set_pps_pdo(index, voltage_mv, current_ma);
        }
    };

} // namespace pocketpd
