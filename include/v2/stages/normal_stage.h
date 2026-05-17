/**
 * @file normal_stage.h
 * @brief Live operating stage for both PPS and fixed PDO chargers.
 */
#pragma once

#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/output_gate.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/stages/normal/fixed_mode.h"
#include "v2/stages/normal/normal_view.h"
#include "v2/stages/normal/pps_mode.h"

namespace pocketpd {

    using Mode = std::variant<FixedMode, PPSMode>;

    class NormalStage : public App::Stage, public App::UseLog<NormalStage> {
    private:
        using Display = tempo::Display;
        using IntervalTimer = tempo::IntervalTimer;

        Display& m_display;
        PdSinkController& m_pd_sink;
        OutputGate& m_output_gate;

        LoadReading m_load_reading{};
        SupplyReading m_supply_reading{};
        bool m_load_init = false;
        bool m_supply_init = false;

        int8_t m_active_pdo_index = -1;
        int8_t m_last_active_index = -1;
        bool m_locked = false;
        Mode m_mode;

        IntervalTimer m_render_interval{40};
        uint8_t m_arrow_frame = 0;
        uint32_t m_last_draw_ms = 0;
        bool m_blink_visible = true;

        // After OFF->ON, the first INA226 read returns a stale conversion (latched while FET
        // was off; observed ~200 mV instead of true VBUS). Discard N load samples and hold the
        // seeded supply value until the sensor has a fresh conversion in hand.
        uint8_t m_postenable_discard_left = 0;
        static constexpr uint8_t POSTENABLE_DISCARD_SAMPLES = 2;

        static constexpr uint32_t READOUT_BLINK_ON_MS = 1200;
        static constexpr uint32_t READOUT_BLINK_OFF_MS = 400;
        static constexpr uint32_t READOUT_BLINK_CYCLE_MS =
            READOUT_BLINK_ON_MS + READOUT_BLINK_OFF_MS;

    public:
        static constexpr const char* LOG_TAG = "Normal";

        NormalStage(Display& display, PdSinkController& pd_sink, OutputGate& output_gate)
            : m_display(display), m_pd_sink(pd_sink), m_output_gate(output_gate) {}

        const char* name() const override {
            return "NORMAL";
        }

        int8_t active_pdo_index() const {
            return m_active_pdo_index;
        }

        bool locked() const {
            return m_locked;
        }

        const Mode& mode() const {
            return m_mode;
        }

        int32_t target_mv() const {
            const auto* p = std::get_if<PPSMode>(&m_mode);
            return p ? p->target_mv : 0;
        }
        int32_t target_ma() const {
            const auto* p = std::get_if<PPSMode>(&m_mode);
            return p ? p->target_ma : 0;
        }
        AdjustMode adjust_mode() const {
            const auto* p = std::get_if<PPSMode>(&m_mode);
            return p ? p->adjust_mode : AdjustMode::VOLTAGE;
        }
        uint8_t voltage_increment_index() const {
            const auto* p = std::get_if<PPSMode>(&m_mode);
            return p ? p->voltage_idx : 0;
        }
        uint8_t current_increment_index() const {
            const auto* p = std::get_if<PPSMode>(&m_mode);
            return p ? p->current_idx : 0;
        }

        void prepare(int8_t pdo_index = -1) {
            m_active_pdo_index = pdo_index;
        }

        void on_enter(Conductor&) override {
            m_locked = false;

            // Turns off output if the profile has changed
            if (m_active_pdo_index != m_last_active_index) {
                m_output_gate.disable();
            }

            if (m_active_pdo_index < 0) {
                m_last_active_index = -1;
                log.info("Entered with no profile selected");
                draw();
                return;
            }

            if (m_pd_sink.is_index_pps(m_active_pdo_index)) {
                enter_pps_profile();
            } else {
                enter_fixed_profile();
            }

            m_last_active_index = m_active_pdo_index;
            draw();
        }

        void on_tick(Conductor&, uint32_t now_ms) override {
            if (m_render_interval.tick(now_ms)) {
                if (m_last_draw_ms != 0) {
                    const uint32_t period = now_ms - m_last_draw_ms;
                    const uint32_t hz = period == 0 ? 0 : 1000 / period;
                    // log.debug("draw period={}ms (~{}Hz)", period, hz);
                }
                m_last_draw_ms = now_ms;
                m_blink_visible = m_output_gate.is_enabled() ||
                                  (now_ms % READOUT_BLINK_CYCLE_MS) < READOUT_BLINK_ON_MS;
                draw();
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const ButtonEvent& event) {
                    // L+R combo always reachable; must precede lock guard so a locked screen can
                    // unlock.
                    if (event.lr_long()) {
                        m_locked = !m_locked;
                        return;
                    }

                    if (m_locked) {
                        return;
                    }

                    if (event.r_short()) {
                        const bool was_enabled = m_output_gate.is_enabled();
                        m_output_gate.toggle();
                        // Display source flips from supply-side to load-side on enable. Seed the
                        // load EMA with the current supply value so the first frame after toggle
                        // shows the known VBUS rather than a stale or zero load_reading.
                        if (!was_enabled && m_output_gate.is_enabled() && m_supply_init) {
                            m_load_reading.vbus_mv = m_supply_reading.mv;
                            m_load_init = true;
                            m_postenable_discard_left = POSTENABLE_DISCARD_SAMPLES;
                        }
                        return;
                    }

                    if (event.r_long()) {
                        conductor.request<EnergyStage>(m_active_pdo_index);
                        return;
                    }

                    if (event.l_long()) {
                        conductor.request<ProfilePickerStage>();
                        return;
                    }

                    // V/I are adjustable in PPS mode
                    if (auto* pps = std::get_if<PPSMode>(&m_mode)) {
                        pps->on_button(event);
                    }
                },
                [&](const EncoderEvent& event) {
                    if (m_locked) {
                        return;
                    }
                    auto* pps = std::get_if<PPSMode>(&m_mode);
                    if (pps == nullptr) {
                        return;
                    }

                    if (!pps->on_encoder(event)) {
                        const auto msg = "set_pps_pdo({}, {}, {}) failed";
                        log.error(msg, m_active_pdo_index, pps->target_mv, pps->target_ma);
                    }
                },
                [&](const SensorEvent& event) {
                    if (m_postenable_discard_left > 0) {
                        --m_postenable_discard_left;
                    } else if (m_load_init) {
                        m_load_reading = m_load_reading.ema(event.load);
                    } else {
                        m_load_reading = event.load;
                        m_load_init = true;
                    }

                    if (event.supply.valid) {
                        if (m_supply_init) {
                            m_supply_reading = m_supply_reading.ema(event.supply);
                        } else {
                            m_supply_reading = event.supply;
                            m_supply_init = true;
                        }
                    }
                },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }

    private:
        /**
         * @brief Enter (or re-enter) a PPS profile. Construct fresh state on profile change or
         * mode switch; otherwise preserve the user's prior target edits and reissue.
         */
        void enter_pps_profile() {
            const bool same_profile = (m_active_pdo_index == m_last_active_index);
            const bool reusable = same_profile && std::holds_alternative<PPSMode>(m_mode);

            if (!reusable) {
                PPSMode pps{m_pd_sink, m_active_pdo_index};
                m_mode = pps;

                const auto msg = "Entered PPS profile pdo_index={} target_mv={} target_ma={}";
                log.info(msg, m_active_pdo_index, pps.target_mv, pps.target_ma);
            }

            auto& pps = std::get<PPSMode>(m_mode);
            if (!pps.apply()) {
                const auto msg = "set_pps_pdo({}, {}, {}) failed";
                log.error(msg, m_active_pdo_index, pps.target_mv, pps.target_ma);
            }
        }

        /**
         * @brief Enter a fixed PDO profile. Trigger `set_pdo` on every entry.
         */
        void enter_fixed_profile() {
            if (!std::holds_alternative<FixedMode>(m_mode)) {
                m_mode = FixedMode{};
            }

            log.info("Entered PDO profile pdo_index={}", m_active_pdo_index);
            if (!m_pd_sink.set_pdo(m_active_pdo_index)) {
                log.error("set_pdo({}) failed", m_active_pdo_index);
            }
        }

        NormalViewModel build_view_model() const {
            NormalViewModel vm = {
                .active_pdo_index = m_active_pdo_index,
                .has_profile = m_active_pdo_index >= 0,
                .output_enabled = m_output_gate.is_enabled(),
                .readout_visible = m_blink_visible,
                .locked = m_locked,
                .arrow_frame = m_arrow_frame,
                .load_reading = m_load_reading,
                .supply_reading = m_supply_reading,
            };

            if (!vm.has_profile) {
                return vm;
            }

            auto handle_mode = tempo::overloaded{
                [&](const FixedMode& mode) {
                    vm.pdo_max_mv = m_pd_sink.pdo_max_voltage_mv(m_active_pdo_index);
                    vm.pdo_max_ma = m_pd_sink.pdo_max_current_ma(m_active_pdo_index);
                },
                [&](const PPSMode& mode) {
                    vm.is_pps = true;
                    vm.target_mv = mode.target_mv;
                    vm.target_ma = mode.target_ma;
                    vm.adjust_mode = mode.adjust_mode;
                    vm.cursor_idx = mode.cursor_index();
                },
            };

            std::visit(handle_mode, m_mode);
            return vm;
        }

        void draw() {
            NormalView::render(m_display, build_view_model());
            if (m_output_gate.is_enabled()) {
                m_arrow_frame = (m_arrow_frame + 1) % pocketpd::bitmap::ARROW_FRAMES.size();
            }
        }
    };

} // namespace pocketpd
