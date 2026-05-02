/**
 * @file boot_stage.h
 * @brief First stage at power-on. Renders the splash screen with the logo and firmware version,
 * then requests `ObtainStage` after `BOOT_TO_OBTAIN_MS`.
 */
#pragma once

#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include <cstdint>

#include "v2/app.h"
#include "v2/images.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class BootStage : public App::Stage, public tempo::UseLog<BootStage> {
    private:
        tempo::Display& m_display;
        tempo::TimeoutTimer m_timeout;

    public:
        static constexpr const char* LOG_TAG = "Boot";

        explicit BootStage(tempo::Display& display) : m_display(display) {}

        const char* name() const override {
            return "BOOT";
        }

        void on_enter(Conductor&) override {
            m_timeout.disarm();
            m_display.clear();
            m_display.draw_bitmap(0, 0, 128 / 8, 64, bitmap::LOGO.data());
            m_display.draw_text(67, 64, "FW: ");
            m_display.draw_text(87, 64, FW_VERSION);
            m_display.flush();
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            if (!m_timeout.armed()) {
                m_timeout.set(now_ms, BOOT_TO_OBTAIN_MS);
                return;
            }
            if (m_timeout.reached(now_ms)) {
                conductor.request<ObtainStage>();
            }
        }
    };

} // namespace pocketpd
