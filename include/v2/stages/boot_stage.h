/**
 * @file boot_stage.h
 * @brief First stage at power-on. Renders the splash screen with the logo and firmware version,
 * then yields after BOOT_TO_OBTAIN_MS.
 *
 * In F1 the timeout simply flips an observable `boot_complete()` flag and logs a single info line;
 * F2 will replace that with `conductor.request<ObtainStage>()`.
 */
#pragma once

#include <tempo/core/time.h>
#include <tempo/hardware/display.h>

#include "v2/app.h"
#include "v2/images.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class BootStage : public App::Stage, public tempo::UseLog<BootStage> {
    private:
        tempo::Display& m_display;
        uint32_t m_deadline_ms = 0;
        bool m_armed = false;
        bool m_boot_complete = false;

    public:
        static constexpr const char* LOG_TAG = "Boot";

        explicit BootStage(tempo::Display& display) : m_display(display) {}

        const char* name() const override {
            return "BOOT";
        }

        void on_enter(Conductor&) override {
            m_display.clear();
            m_display.draw_bitmap(0, 0, 128 / 8, 64, bitmap::LOGO.data());
            m_display.draw_text(67, 64, "FW: ");
            m_display.draw_text(87, 64, FW_VERSION);
            m_display.flush();
        }

        void on_tick(Conductor&, uint32_t now_ms) override {
            if (!m_armed) {
                m_deadline_ms = now_ms + BOOT_TO_OBTAIN_MS;
                m_armed = true;
                return;
            }
            if (m_boot_complete) {
                return;
            }
            if (tempo::time_reached(now_ms, m_deadline_ms)) {
                m_boot_complete = true;
                log.info("Boot splash complete");
            }
        }

        bool boot_complete() const {
            return m_boot_complete;
        }
    };

} // namespace pocketpd
