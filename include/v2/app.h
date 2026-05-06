/**
 * @file app.h
 * @brief PocketPD v2 application alias.
 *
 * Provides the `App = tempo::Application<Event, Stages...>` alias used as the
 * single source of truth for stage and task base types throughout v2.
 */
#pragma once

#include <tempo/tempo.h>

#include "v2/events.h"


namespace pocketpd {

    // —— Forward declare the stages in this application

    class BootStage;
    class ObtainStage;
    class ProfilePickerStage;
    class NormalStage;

    // —— Can also define events here as well

    // —— Define common types

    enum class ProfilePickerMode : uint8_t {
        REVIEW,
        SELECT,
    };

    // —— Define the application alias which is a combination of Events and Stages

    using App = tempo::Application<Event, BootStage, ObtainStage, ProfilePickerStage, NormalStage>;
} // namespace pocketpd

/*
 * Stage headers go below the `App` alias on purpose.
 *
 * The cycle: each stage header includes `app.h` because stages inherit
 * `App::Stage` and call `App::Conductor::request<>()`. But `App` itself
 * lists the stage types in its template parameters, so it needs to know
 * the stages exist. If `app.h` included the stage headers at the top,
 * those headers would re-include `app.h` before `App` was defined, and
 * neither side could compile.
 *
 * The two-step fix:
 *   1. Forward-declare the stages above (`class BootStage;` etc.). That is
 *      enough for the `App` alias — it only needs the names, not the full
 *      class bodies.
 *   2. Include the real stage headers here, after `App` is defined. When
 *      they re-include `app.h`, the alias already exists, so they can
 *      inherit from `App::Stage` without trouble.
 *
 * The headers stay in this file (instead of a separate "all stages"
 * header) so a single `#include "v2/app.h"` gives callers both the alias
 * and the concrete stage types in one shot.
 */
#include "v2/stages/boot_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/obtain_stage.h"
#include "v2/stages/profile_picker_stage.h"
