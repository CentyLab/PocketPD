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

    // —— Stage forward declarations

    class BootStage;
    class ObtainStage;

    // —— Application alias

    using App = tempo::Application<Event, BootStage, ObtainStage>;

} // namespace pocketpd
