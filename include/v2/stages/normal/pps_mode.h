/**
 * @file pps_mode.h
 * @brief PPS sub-mode for NormalStage. Live mV/mA targets + encoder edits.
 *
 */
#pragma once

#include <algorithm>
#include <cstdint>

#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/state.h"

namespace pocketpd {

    struct PPSMode {

        constexpr static int32_t MIN_CURRENT_MA = 1000;
        int32_t target_mv = 5000;
        int32_t target_ma = 1000;
        uint8_t voltage_idx = 0;
        uint8_t current_idx = 0;
        AdjustMode adjust_mode = AdjustMode::VOLTAGE;

        /**
         * @brief Bind to a PD sink + PDO index. Targets initializes at 5 V / 1 A, then clamp into
         * the PDO's advertised range.
         */
        PPSMode(PdSinkController& sink, int8_t pdo_idx) : m_sink(&sink), m_pdo_idx(pdo_idx) {
            clamp_to_pdo();
        }

        /**
         * @brief Handle a button gesture on the PPS branch only.
         *
         * - L SHORT toggle voltage/current adjustment
         * - ENCODER SHORT cycle the active increment table index.
         *
         * Stage-level gestures (R SHORT output toggle, L LONG → ProfilePicker) are filtered
         * upstream and never reach this method.
         */
        void on_button(const ButtonEvent& evt) {
            if (evt.id == ButtonId::L && evt.gesture == Gesture::SHORT) {
                adjust_mode = (adjust_mode == AdjustMode::VOLTAGE) ? AdjustMode::CURRENT
                                                                   : AdjustMode::VOLTAGE;
                return;
            }
            if (evt.id == ButtonId::ENCODER && evt.gesture == Gesture::SHORT) {
                if (adjust_mode == AdjustMode::VOLTAGE) {
                    voltage_idx = (voltage_idx + 1) % VOLTAGE_INCREMENTS_MV.size();
                } else {
                    current_idx = (current_idx + 1) % CURRENT_INCREMENTS_MA.size();
                }
            }
        }

        /**
         * @brief Apply an encoder delta to the focused target and reissue.
         *
         * @return Result of the underlying `set_pps_pdo` call. `true` when the
         * delta is zero (no reissue needed). `false` only on actual sink
         * failure.
         */
        [[nodiscard]] bool on_encoder(const EncoderEvent& evt) {
            if (evt.delta == 0) {
                return true;
            }

            apply_encoder_delta(-evt.delta);
            return apply();
        }

        /**
         * @brief Clamp the current target to PDO + step grid and push to sink.
         *
         * @return Result of `set_pps_pdo`. Stage log on `false`.
         */
        [[nodiscard]] bool apply() {
            clamp_to_pdo();
            return m_sink->set_pps_pdo(m_pdo_idx, target_mv, target_ma);
        }

        uint8_t cursor_index() const {
            return (adjust_mode == AdjustMode::VOLTAGE) ? voltage_idx : current_idx;
        }

    private:
        PdSinkController* m_sink;
        int8_t m_pdo_idx;

        void apply_encoder_delta(int delta) {
            if (adjust_mode == AdjustMode::VOLTAGE) {
                const int32_t step = VOLTAGE_INCREMENTS_MV.at(voltage_idx);
                target_mv += delta * step;
            } else {
                const int32_t step = CURRENT_INCREMENTS_MA.at(current_idx);
                target_ma += delta * step;
                target_ma = std::max(target_ma, MIN_CURRENT_MA);
            }
        }

        void clamp_to_pdo() {
            const int32_t v_lo = m_sink->pdo_min_voltage_mv(m_pdo_idx);
            const int32_t v_hi = m_sink->pdo_max_voltage_mv(m_pdo_idx);
            const int32_t a_hi = m_sink->pdo_max_current_ma(m_pdo_idx);

            if (v_lo > 0 && v_hi > 0) {
                target_mv = std::clamp(target_mv, v_lo, v_hi);
                target_mv -= target_mv % PPS_VOLTAGE_STEP_MV;
            }
            if (a_hi > 0) {
                target_ma = std::clamp(target_ma, int32_t{0}, a_hi);
                target_ma -= target_ma % PPS_CURRENT_STEP_MA;
            }
        }
    };

} // namespace pocketpd
