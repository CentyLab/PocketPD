#pragma once

#include <cstdint>
#include <deque>

#include <gmock/gmock.h>

#include "v2/hal/power_monitor.h"

namespace pocketpd {

    class MockPowerMonitor : public PowerMonitor {
    public:
        MOCK_METHOD(void, begin, (), (override));
        MOCK_METHOD(PowerReading, read, (), (override));
        MOCK_METHOD(void, set_alert_threshold_ma, (uint32_t ma), (override));
    };

    /**
     * @brief Scripted PowerMonitor for tests that want to feed a sequence of readings.
     *
     * `read()` pops the front of the queue if non-empty; once drained it returns the
     * last queued reading instead of a default-constructed one, so tests that tick
     * past the script length stay stable.
     */
    class FakePowerMonitor : public PowerMonitor {
    private:
        std::deque<PowerReading> m_queue;
        PowerReading m_last;
        uint32_t m_threshold_ma = 0;
        bool m_began = false;

    public:
        void push(PowerReading r) {
            m_queue.push_back(r);
            m_last = r;
        }

        bool began() const {
            return m_began;
        }

        uint32_t alert_threshold_ma() const {
            return m_threshold_ma;
        }

        void begin() override {
            m_began = true;
        }

        PowerReading read() override {
            if (m_queue.empty()) {
                return m_last;
            }
            PowerReading r = m_queue.front();
            m_queue.pop_front();
            return r;
        }

        void set_alert_threshold_ma(uint32_t ma) override {
            m_threshold_ma = ma;
        }
    };

} // namespace pocketpd
