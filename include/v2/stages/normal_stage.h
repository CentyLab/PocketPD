/**
 * @file normal_stage.h
 * @brief Live operating stage for both PPS and fixed PDO chargers.
 *
 * Profile (`PPS` vs `PDO`) is set at entry by the caller via
 * `Conductor::request<NormalStage>(Profile)` which forwards to
 * `prepare(Profile)`. The profile is locked for the duration of the stage —
 * a long-press of the L button exits back to ProfilePicker SELECT so the
 * user can pick a different profile.
 *
 * Stub today: enters and logs the chosen profile. The full encoder edit
 * (PPS-only), PDO/PPS RDO issuance, output toggle, autosave, sensor read,
 * and energy accumulator land once those subsystems arrive.
 */
#pragma once

#include <tempo/bus/visit.h>

#include <variant>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class ProfilePickerStage;

    class NormalStage : public App::Stage, public App::UseLog<NormalStage> {
    private:
        Profile m_profile = Profile::PDO;

    public:
        static constexpr const char* LOG_TAG = "Normal";

        const char* name() const override {
            return "NORMAL";
        }

        Profile profile() const {
            return m_profile;
        }

        void prepare(Profile profile) {
            m_profile = profile;
        }

        void on_enter(Conductor&) override {
            log.info("entered profile={}", m_profile == Profile::PPS ? "PPS" : "PDO");
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override;
    };

} // namespace pocketpd
