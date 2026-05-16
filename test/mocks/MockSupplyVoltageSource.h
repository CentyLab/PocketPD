#pragma once

#include <deque>

#include <gmock/gmock.h>

#include "v2/hal/supply_voltage_source.h"

namespace pocketpd {

    class MockSupplyVoltageSource : public SupplyVoltageSource {
    public:
        MOCK_METHOD(void, begin, (), (override));
        MOCK_METHOD(SupplyVoltageReading, read, (), (override));
    };

    /**
     * @brief Scripted SupplyVoltageSource. Same shape as FakePowerMonitor: pops
     * the queue each `read()`, returns the last queued reading once drained.
     */
    class FakeSupplyVoltageSource : public SupplyVoltageSource {
    private:
        std::deque<SupplyVoltageReading> m_queue;
        SupplyVoltageReading m_last;
        bool m_began = false;

    public:
        void push(SupplyVoltageReading r) {
            m_queue.push_back(r);
            m_last = r;
        }

        bool began() const {
            return m_began;
        }

        void begin() override {
            m_began = true;
        }

        SupplyVoltageReading read() override {
            if (m_queue.empty()) {
                return m_last;
            }
            SupplyVoltageReading r = m_queue.front();
            m_queue.pop_front();
            return r;
        }
    };

} // namespace pocketpd
