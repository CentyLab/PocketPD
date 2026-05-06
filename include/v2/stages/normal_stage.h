/**
 * @file normal_stage.h
 * @brief Live operating stage for both PPS and fixed PDO chargers. Carries the
 * display, PD sink, and output-gate dependencies needed to issue the entry RDO,
 * toggle the load switch on R-SHORT, and render the live snapshot. L-LONG exits
 * back to ProfilePicker SELECT.
 */
#pragma once

#include <tempo/bus/visit.h>
#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include <cstdio>
#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/output_gate.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/state.h"

namespace pocketpd {

    class NormalStage : public App::Stage, public App::UseLog<NormalStage> {
    private:
        using Display = tempo::Display;

        Display& m_display;
        PdSinkController& m_pd_sink;
        OutputGate& m_output_gate;

        Profile m_profile = Profile::PDO;
        uint8_t m_active_pdo_index = 0;
        SensorSnapshot m_snapshot{};
        tempo::IntervalTimer m_render_gate{33};

        void draw();

    public:
        static constexpr const char* LOG_TAG = "Normal";

        NormalStage(Display& display, PdSinkController& pd_sink, OutputGate& output_gate)
            : m_display(display), m_pd_sink(pd_sink), m_output_gate(output_gate) {}

        const char* name() const override {
            return "NORMAL";
        }

        Profile profile() const {
            return m_profile;
        }

        uint8_t active_pdo_index() const {
            return m_active_pdo_index;
        }

        SensorSnapshot& sensor_snapshot() {
            return m_snapshot;
        }
        const SensorSnapshot& sensor_snapshot() const {
            return m_snapshot;
        }

        void prepare(Profile profile = Profile::PDO, uint8_t pdo_index = 0) {
            m_profile = profile;
            m_active_pdo_index = pdo_index;
        }

        void on_enter(Conductor&) override {
            log.info(
                "entered profile={} pdo_index={}",
                m_profile == Profile::PPS ? "PPS" : "PDO",
                m_active_pdo_index
            );

            if (m_profile == Profile::PDO) {
                if (!m_pd_sink.set_pdo(m_active_pdo_index)) {
                    log.error("set_pdo({}) failed", m_active_pdo_index);
                }
            }

            draw();
        }

        void on_tick(Conductor&, uint32_t now_ms) override {
            if (m_render_gate.tick(now_ms)) {
                draw();
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::R && evt.gesture == Gesture::SHORT) {
                        if (m_output_gate.is_enabled()) {
                            m_output_gate.disable();
                        } else {
                            m_output_gate.enable();
                        }
                    } else if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.request<ProfilePickerStage>(ProfilePickerMode::SELECT);
                    }
                },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }
    };

    inline void NormalStage::draw() {
        m_display.clear();

        char buffer[16];

        m_display.draw_text(1, 14, "V");
        const auto v_int = m_snapshot.vbus_mv / 1000;
        const auto v_frac = (m_snapshot.vbus_mv % 1000) / 10;
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%lu.%02lu",
            static_cast<unsigned long>(v_int),
            static_cast<unsigned long>(v_frac)
        );
        const auto v_width = m_display.text_width(buffer);
        m_display.draw_text(static_cast<uint8_t>(75 - v_width), 14, buffer);

        m_display.draw_text(1, 47, "A");
        const auto a_int = m_snapshot.current_ma / 1000;
        const auto a_frac = (m_snapshot.current_ma % 1000) / 10;
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%lu.%02lu",
            static_cast<unsigned long>(a_int),
            static_cast<unsigned long>(a_frac)
        );
        const auto a_width = m_display.text_width(buffer);
        m_display.draw_text(static_cast<uint8_t>(75 - a_width), 47, buffer);

        std::snprintf(buffer, sizeof(buffer), "[%u]", static_cast<unsigned>(m_active_pdo_index));
        m_display.draw_text(95, 50, buffer);
        m_display.draw_text(110, 62, "PDO");

        m_display.draw_text(90, 14, m_output_gate.is_enabled() ? "ON" : "OFF");

        m_display.flush();
    }

} // namespace pocketpd
