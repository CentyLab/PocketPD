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
#include "v2/stages/normal/mode.h"
#include "v2/stages/normal/normal_view.h"

namespace pocketpd {

    class NormalStage : public App::Stage,
                        public App::UseLog<NormalStage>,
                        public App::UsePublisher<NormalStage> {
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
        Mode m_mode;
        
        IntervalTimer m_render_interval{40};
        uint8_t m_arrow_frame = 0;
        uint32_t m_last_draw_ms = 0;
        bool m_blink_visible = true;
        
        bool m_locked = false;
        int32_t m_comp_offset_mv = 0;
        
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

        int32_t comp_offset_mv() const {
            return m_comp_offset_mv;
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

        void on_enter(Conductor&, uint32_t) override {
            m_locked = false;

            int count = m_pd_sink.pdo_count();
            if (count == 0) {
                m_mode = PassthroughMode{};
                publish(PpsTargetEvent{-1, 0, 0});
                draw();
                return;
            }

            // Set active PDO index to 0 if it has not been set before
            if (m_active_pdo_index < 0) {
                m_active_pdo_index = 0;
            }

            // Turns off output if the profile has changed
            if (m_active_pdo_index != m_last_active_index) {
                m_output_gate.disable();
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
                        conductor.request<MenuStage>();
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

                    pps->on_encoder(event);
                    publish(PpsTargetEvent{m_active_pdo_index, pps->target_mv, pps->target_ma});
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
                [&](const CompStateEvent& evt) {
                    m_comp_offset_mv = evt.offset_mv;
                },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }

    private:
        void enter_pps_profile() {
            bool same_profile = m_active_pdo_index == m_last_active_index;
            if (!same_profile) {
                PPSMode pps{m_pd_sink, m_active_pdo_index};
                m_mode = pps;

                const auto msg = "Entered PPS profile pdo_index={} target_mv={} target_ma={}";
                log.info(msg, m_active_pdo_index, pps.target_mv, pps.target_ma);
            }

            auto& pps = std::get<PPSMode>(m_mode);
            pps.clamp();
            publish(PpsTargetEvent{m_active_pdo_index, pps.target_mv, pps.target_ma});
        }

        void enter_fixed_profile() {
            m_mode = FixedMode{
                .pdo_max_mv = m_pd_sink.pdo_max_voltage_mv(m_active_pdo_index),
                .pdo_max_ma = m_pd_sink.pdo_max_current_ma(m_active_pdo_index),
            };

            log.info("Entered PDO profile pdo_index={}", m_active_pdo_index);
            if (!m_pd_sink.set_pdo(m_active_pdo_index)) {
                log.error("set_pdo({}) failed", m_active_pdo_index);
            }
            publish(PpsTargetEvent{-1, 0, 0});
        }

        NormalViewModel build_view_model() const {
            return NormalViewModel{
                .active_pdo_index = m_active_pdo_index,
                .output_enabled = m_output_gate.is_enabled(),
                .readout_visible = m_blink_visible,
                .locked = m_locked,
                .arrow_frame = m_arrow_frame,
                .load_reading = m_load_reading,
                .supply_reading = m_supply_reading,
                .mode = m_mode,
                .comp_offset_mv = m_comp_offset_mv,
            };
        }

        void draw() {
            NormalView::render(m_display, build_view_model());
            if (m_output_gate.is_enabled()) {
                m_arrow_frame = (m_arrow_frame + 1) % pocketpd::bitmap::ARROW_FRAMES.size();
            }
        }
    };

} // namespace pocketpd
