/**
 * @file pdo_picker_stage.h
 * @brief PDO picker / capability list stage. Two modes via tempo
 * payload-passing transition: REVIEW renders the source PDO list and
 * waits for input; SELECT will host cursor + commit logic in a later PR.
 *
 * REVIEW renders once on entry — the PDO list is fixed for the lifetime
 * of the negotiation. No on_tick, no input handling yet; transitions out
 * land with F3b.
 */
#pragma once

#include <tempo/hardware/display.h>

#include <cstdint>
#include <cstdio>

#include "v2/app.h"
#include "v2/hal/pd_sink_controller.h"

namespace pocketpd {

    void render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink);

    class PdoPickerStage : public App::Stage, public tempo::UseLog<PdoPickerStage> {
    public:
        enum class Mode : uint8_t { REVIEW, SELECT };

    private:
        tempo::Display& m_display;
        PdSinkController& m_pd_sink;
        Mode m_pending_mode = Mode::REVIEW;

    public:
        static constexpr const char* LOG_TAG = "PdoPick";

        PdoPickerStage(tempo::Display& display, PdSinkController& pd_sink)
            : m_display(display), m_pd_sink(pd_sink) {}

        const char* name() const override {
            return "PDO_PICKER";
        }

        Mode mode() const {
            return m_pending_mode;
        }

        void prepare(Mode mode) {
            m_pending_mode = mode;
        }

        void on_enter(Conductor&) override {
            if (m_pending_mode == Mode::REVIEW) {
                render_pdo_list(m_display, m_pd_sink);
            } else {
                log.info("entered mode=SELECT");
            }
        }
    };

    inline void render_pdo_list(tempo::Display& display, const PdSinkController& pd_sink) {
        display.clear();
        const int count = pd_sink.pdo_count();
        if (count == 0) {
            display.draw_text(8, 34, "No Profile Detected");
            display.flush();
            return;
        }

        std::array<char, 32> buffer{};
        for (int i = 0; i < count; ++i) {
            const auto y = static_cast<uint8_t>(9 * (i + 1));
            if (pd_sink.is_index_fixed(i)) {
                const int v = pd_sink.pdo_max_voltage_mv(i) / 1000;
                const int a = pd_sink.pdo_max_current_ma(i) / 1000;
                std::snprintf(buffer.data(), buffer.size(), "PDO: %dV @ %dA", v, a);
                display.draw_text(5, y, buffer.data());
            } else if (pd_sink.is_index_pps(i)) {
                const int min_mv = pd_sink.pdo_min_voltage_mv(i);
                const int max_mv = pd_sink.pdo_max_voltage_mv(i);
                
                const int min_v = min_mv / 1000;
                const int min_dv = (min_mv % 1000) / 100;
                
                const int max_v = max_mv / 1000;
                const int max_dv = (max_mv % 1000) / 100;
                
                const int a = pd_sink.pdo_max_current_ma(i) / 1000;
                std::snprintf(
                    buffer.data(),
                    buffer.size(),
                    "PPS: %d.%dV~%d.%dV @ %dA",
                    min_v,
                    min_dv,
                    max_v,
                    max_dv,
                    a
                );
                display.draw_text(5, y, buffer.data());
            }
        }

        display.flush();
    }

} // namespace pocketpd
