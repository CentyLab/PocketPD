// src/tempo/tempo.h
//
// Umbrella include for the tempo framework. Pull this header to get the
// common public surface. For fine-grained control, include the individual
// module headers instead.
#pragma once

// core
#include "tempo/core/panic.h"
#include "tempo/core/time.h"

// hardware (interfaces)
#include "tempo/hardware/button_input.h"
#include "tempo/hardware/encoder_input.h"
#include "tempo/hardware/display.h"
#include "tempo/hardware/stream.h"

// bus
#include "tempo/bus/event_queue.h"
#include "tempo/bus/event.h"
#include "tempo/bus/publisher.h"
#include "tempo/bus/visit.h"

// stage
#include "tempo/stage/stage.h"
#include "tempo/stage/conductor.h"
#include "tempo/stage/stage_mask.h"

// sched
#include "tempo/sched/background_task.h"
#include "tempo/sched/cooperative_scheduler.h"
#include "tempo/sched/task.h"
#include "tempo/sched/periodic_task.h"
#include "tempo/sched/stage_scoped_task.h"

// diag
#include "tempo/diag/logger.h"

// app
#include "tempo/app/application.h"
