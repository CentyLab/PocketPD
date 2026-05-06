/**
 * @file normal_stage.h
 * @brief Live operating stage for both PPS and fixed PDO chargers.
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
#include "v2/state.h"

namespace pocketpd {

    class NormalStage : public App::Stage, public App::UseLog<NormalStage> {
    private:
        using Display = tempo::Display;

        Display& m_display;
        PdSinkController& m_pd_sink;
        OutputGate& m_output_gate;
        SensorSnapshot m_snapshot{};

        int8_t m_active_pdo_index = -1;
        tempo::IntervalTimer m_render_interval{33};

        void draw();

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

        void prepare(int8_t pdo_index = -1) {
            m_active_pdo_index = pdo_index;
        }

        void on_enter(Conductor&) override {
            if (m_active_pdo_index < 0) {
                log.info("Entered with no profile selected");
                draw();
                return;
            }

            if (m_pd_sink.is_index_pps(m_active_pdo_index)) {
                log.info("Entered PPS profile pdo_index={}", m_active_pdo_index);
            } else {
                log.info("Entered PDO profile pdo_index={}", m_active_pdo_index);
                if (!m_pd_sink.set_pdo(m_active_pdo_index)) {
                    log.error("set_pdo({}) failed", m_active_pdo_index);
                }
            }

            draw();
        }

        void on_tick(Conductor&, uint32_t now_ms) override {
            if (m_render_interval.tick(now_ms)) {
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
                [&](const SensorEvent& evt) { m_snapshot = evt.snapshot; },
                [](const auto&) {},
            };

            std::visit(handler, event);
        }
    };

    inline void NormalStage::draw() {
        m_display.clear();

        if (m_active_pdo_index < 0) {
            m_display.set_font(tempo::Font::BASE);
            m_display.draw_text(8, 34, "No Profile Selected");
            m_display.flush();
            return;
        }

        std::array<char, 32> buffer{};

        auto draw_line = [&](const char* label, uint8_t y, uint32_t value) {
            m_display.draw_text(1, y, label);

            const unsigned long whole = value / 1000;
            const unsigned long fraction = (value % 1000) / 10;
            std::snprintf(buffer.data(), buffer.size(), "%lu.%02lu", whole, fraction);

            const auto w = m_display.text_width(buffer.data());
            m_display.draw_text(static_cast<uint8_t>(75 - w), y, buffer.data());
        };

        m_display.set_font(tempo::Font::XL);
        draw_line("V", 14, m_snapshot.vbus_mv);
        draw_line("A", 47, m_snapshot.current_ma);

        m_display.set_font(tempo::Font::BASE);
        std::snprintf(buffer.data(), buffer.size(), "[%u]", m_active_pdo_index);
        m_display.draw_text(110, 50, buffer.data());

        m_display.draw_text(110, 64, m_pd_sink.is_index_pps(m_active_pdo_index) ? "PPS" : "PDO");
        m_display.draw_text(90, 14, m_output_gate.is_enabled() ? "ON" : "OFF");

        m_display.flush();
    }

} // namespace pocketpd
