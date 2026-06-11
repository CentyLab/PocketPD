/**
 * @file pps_control_task.h
 * @brief Owns PPS sink writes during NormalStage: applies user target + cable comp offset.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/output_gate.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/preferences_store.h"

namespace pocketpd {

    class PpsControlTask : public App::StageScopedTask,
                           public App::UseLog<PpsControlTask>,
                           public App::UsePublisher<PpsControlTask> {
    private:
        static constexpr int STEP_MV = PPS_VOLTAGE_STEP_MV;
        static constexpr int DEAD_BAND_MV = 20;
        static constexpr int MAX_COMP_MV = 500;

        const PreferencesStore& m_prefs;
        OutputGate& m_gate;
        PdSinkController& m_sink;

        int m_pdo_index = -1;
        int m_target_mv = 0;
        int m_target_ma = 0;
        int m_comp_offset_mv = 0;

        LoadReading m_load{};
        bool m_load_init = false;

    public:
        static constexpr uint32_t PERIOD_MS = 125;
        static constexpr const char* LOG_TAG = "PpsCtrl";

        PpsControlTask(const PreferencesStore& prefs, OutputGate& gate, PdSinkController& sink)
            : App::StageScopedTask(PERIOD_MS, App::StageMask::of<NormalStage>()),
              m_prefs(prefs),
              m_gate(gate),
              m_sink(sink) {}

        const char* name() const override {
            return LOG_TAG;
        }

        int comp_offset_mv() const {
            return m_comp_offset_mv;
        }

        void on_event(const Event& event, uint32_t) override {
            if (const auto* pps = std::get_if<PpsTargetEvent>(&event)) {
                const bool pdo_changed = pps->pdo_index != m_pdo_index;
                m_pdo_index = pps->pdo_index;
                m_target_mv = pps->target_mv;
                m_target_ma = pps->target_ma;
                if (pdo_changed) {
                    const bool had_offset = m_comp_offset_mv != 0;
                    m_comp_offset_mv = 0;
                    m_load_init = false;
                    if (had_offset) {
                        publish(CompStateEvent{0});
                    }
                }

                write_request();
                return;
            }

            if (const auto* sensor = std::get_if<SensorEvent>(&event)) {
                m_load = sensor->load;
                m_load_init = true;
            }
        }

        void on_tick(uint32_t) override {
            if (!m_prefs.get().voltage_comp_enabled) {
                clear_offset();
                return;
            }

            if (m_pdo_index < 0) {
                return;
            }

            // Output off: user may swap cable or load, so the learned offset is stale.
            if (!m_gate.is_enabled()) {
                clear_offset();
                return;
            }

            if (!m_load_init) {
                return;
            }

            const int error = m_target_mv - static_cast<int>(m_load.vbus_mv);
            if (std::abs(error) <= DEAD_BAND_MV) {
                return;
            }

            const int delta = (error > 0) ? STEP_MV : -STEP_MV;
            int max_offset = MAX_COMP_MV;
            if (delta > 0) {
                const int pdo_max = m_sink.pdo_max_voltage_mv(m_pdo_index);
                const int headroom = std::max(0, pdo_max - m_target_mv);
                max_offset = std::min(MAX_COMP_MV, headroom);
            }

            const int new_offset = std::clamp(m_comp_offset_mv + delta, 0, max_offset);
            if (new_offset == m_comp_offset_mv) {
                return;
            }

            const int prev_offset = m_comp_offset_mv;
            m_comp_offset_mv = new_offset;
            if (write_request()) {
                publish(CompStateEvent{m_comp_offset_mv});
            } else {
                m_comp_offset_mv = prev_offset;
            }
        }

    private:
        void clear_offset() {
            if (m_comp_offset_mv == 0) {
                return;
            }

            m_comp_offset_mv = 0;
            if (write_request()) {
                publish(CompStateEvent{0});
            }
        }

        bool write_request() {
            if (m_pdo_index < 0) {
                return false;
            }

            const int offset = m_prefs.get().voltage_comp_enabled ? m_comp_offset_mv : 0;
            const int request_mv = m_target_mv + offset;
            if (!m_sink.set_pps_pdo(m_pdo_index, request_mv, m_target_ma)) {
                log.error(
                    "ppsctrl set_pps_pdo({}, {}, {}) failed", m_pdo_index, request_mv, m_target_ma
                );

                return false;
            }

            return true;
        }
    };

} // namespace pocketpd
