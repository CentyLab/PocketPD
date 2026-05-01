/**
 * @file obtain_stage.h
 * @brief Stage that runs PD negotiation, learns the source PDO list, and
 * announces the result to the rest of the app via `PdReadyEvent`.
 *
 * Issue #33: ObtainStage must NOT issue an RDO request. The first request runs
 * later (NormalPpsStage / NormalPdoStage) once EEPROM has been consulted. Until
 * then the AP33772 holds whatever default profile the source negotiated at
 * begin().
 */
#pragma once

#include <array>
#include <cstdint>

#include <tempo/bus/publisher.h>
#include <tempo/core/time.h>

#include "AP33772_debug.h"

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"

namespace pocketpd {

    class ObtainStage : public App::Stage, public tempo::UseLog<ObtainStage> {
    private:
        PdSinkController& m_pd_sink;
        tempo::Publisher<Event>& m_publisher;
        tempo::IntervalTimer m_dump_timer{1000};

    public:
        static constexpr const char* LOG_TAG = "Obtain";

        ObtainStage(PdSinkController& pd_sink, tempo::Publisher<Event>& publisher)
            : m_pd_sink(pd_sink), m_publisher(publisher) {}

        const char* name() const override {
            return "OBTAIN";
        }

        void on_enter(Conductor&) override {
            if (!m_pd_sink.begin()) {
                log.error("PD negotiation failed");
                return;
            }

            const auto pdo_n = static_cast<uint8_t>(m_pd_sink.pdo_count());
            const auto pps_n = static_cast<uint8_t>(m_pd_sink.pps_count());
            m_publisher.publish(PdReadyEvent{pdo_n, pps_n});

            log.info("PD ready: %u PDO (%u PPS)", pdo_n, pps_n);
        }

        void on_tick(Conductor&, uint32_t now_ms) override {
            if (!m_dump_timer.tick(now_ms)) {
                return;
            }
            std::array<char, 2048> buffer{};
            ap33772::format_pdo(m_pd_sink, buffer.data(), buffer.size());
            log.info(buffer.data());
        }
    };

} // namespace pocketpd
