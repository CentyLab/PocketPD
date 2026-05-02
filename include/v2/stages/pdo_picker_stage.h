/**
 * @file pdo_picker_stage.h
 * @brief Stub stage; transition target for ObtainStage. Picker UI lands in F4.
 *
 * Carries the `Mode` enum so callers can already pass it via
 * `Conductor::request<PdoPickerStage>(Mode)`. Today `prepare(Mode)` stashes
 * the value and `on_enter` logs it; nothing is rendered and no events are
 * handled.
 */
#pragma once

#include <cstdint>

#include "v2/app.h"

namespace pocketpd {

    class PdoPickerStage : public App::Stage, public tempo::UseLog<PdoPickerStage> {
    public:
        enum class Mode : uint8_t { REVIEW, SELECT };

    private:
        Mode m_pending_mode = Mode::REVIEW;

    public:
        static constexpr const char* LOG_TAG = "PdoPick";

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
            log.info("entered mode=%u", static_cast<unsigned>(m_pending_mode));
        }
    };

} // namespace pocketpd
