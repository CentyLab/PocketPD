/**
 * @file obtain_stage.h
 * @brief Stage that runs PD negotiation, learns the source PDO list, and
 * announces the result to the rest of the app via `PdReadyEvent`.
 *
 * After the announce, ObtainStage acts as a short window to handle user input:
 *  1. a short button press resumes Normal operation immediately,
 *  2. encoder rotation jumps to PdoPicker in SELECT mode, and
 *  3. otherwise the stage times out into PdoPicker REVIEW so the PDO list can be reviewed.
 *
 * Issue #33: ObtainStage must NOT issue an RDO request. The first request runs later
 * (NormalPpsStage / NormalPdoStage) once EEPROM has been consulted. Until then the AP33772 holds
 * whatever default profile the source negotiated at begin().
 */
#pragma once

#include <tempo/bus/publisher.h>
#include <tempo/bus/visit.h>
#include <tempo/core/time.h>

#include <array>
#include <cstdint>
#include <variant>

#include "AP33772_debug.h"
#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/pdo_picker_stage.h"

namespace pocketpd {

    class ObtainStage : public App::Stage,
                        public App::UseLog<ObtainStage>,
                        public App::UsePublisher<ObtainStage> {
    private:
        PdSinkController& m_pd_sink;
        bool m_pd_ready = false;

        tempo::IntervalTimer m_dump_timer{3000};
        tempo::TimeoutTimer m_timeout;

    public:
        static constexpr const char* LOG_TAG = "Obtain";

        explicit ObtainStage(PdSinkController& pd_sink) : m_pd_sink(pd_sink) {}

        const char* name() const override {
            return "OBTAIN";
        }

        void on_enter(Conductor&) override {
            m_timeout.disarm();
            m_pd_ready = false;

            if (!m_pd_sink.begin()) {
                log.error("PD negotiation failed");
                return;
            }

            m_pd_ready = true;
            const auto pdo_n = static_cast<uint8_t>(m_pd_sink.pdo_count());
            const auto pps_n = static_cast<uint8_t>(m_pd_sink.pps_count());
            publish(PdReadyEvent{pdo_n, pps_n});

            log.info("PD ready: {} PDO ({} PPS)", pdo_n, pps_n);
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            // Periodically dump the PDO list for debugging
            if (m_dump_timer.tick(now_ms)) {
                std::array<char, 2048> buffer{};
                ap33772::format_pdo(m_pd_sink, buffer.data(), buffer.size());
                log.info("{}", buffer.data());
            }

            if (!m_timeout.armed()) {
                m_timeout.set(now_ms, OBTAIN_TO_PDOPICKER_MS);
                return;
            }

            if (m_timeout.reached(now_ms)) {
                conductor.request<PdoPickerStage>(PdoPickerStage::Mode::REVIEW);
                return; // Should return after stage change request to avoid executing more code
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            
            auto handler = tempo::overloaded{
                [&](const ButtonEvent& evt) {
                    if (evt.gesture == Gesture::SHORT && m_pd_ready) {
                        const Profile profile =
                            m_pd_sink.pps_count() > 0 ? Profile::PPS : Profile::PDO;
                        conductor.request<NormalStage>(profile);
                    }
                },
                [&](const EncoderEvent& evt) {
                    if (evt.delta != 0) {
                        conductor.request<PdoPickerStage>(PdoPickerStage::Mode::SELECT);
                    }
                },
                [](const auto&) {
                    // handle std::monostate and any unrelated event variants
                },
            };

            std::visit(handler, event);
        }
    };

} // namespace pocketpd
