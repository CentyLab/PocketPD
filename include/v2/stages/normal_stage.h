/**
 * @file normal_stage.h
 * @brief Live operating stage for both PPS and fixed PDO chargers.
 *
 * Profile (`PPS` vs `PDO`) is set at entry by the caller via
 * `Conductor::request<NormalStage>(Profile)` which forwards to
 * `prepare(Profile)`. The profile is locked for the duration of the stage —
 * to switch profile, exit to PdoPicker SELECT and re-enter.
 *
 * Stub today: enters and logs the chosen profile. The full encoder edit
 * (PPS-only), PDO/PPS RDO issuance, output toggle, autosave, sensor read,
 * and energy accumulator land once those subsystems arrive.
 */
#pragma once

#include "v2/app.h"
#include "v2/pocketpd.h"

namespace pocketpd {

    class NormalStage : public App::Stage, public tempo::UseLog<NormalStage> {
    private:
        // Still don't know if we should use pending variable or not. TBD.
        Profile m_pending_profile = Profile::PDO;
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
            m_pending_profile = profile;
        }

        void on_enter(Conductor&) override {
            m_profile = m_pending_profile;
            log.info("entered profile=%s", m_profile == Profile::PPS ? "PPS" : "PDO");
        }
    };

} // namespace pocketpd
