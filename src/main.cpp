#include <Arduino.h>

#include <cmath>

#include "tempo/diag/logger.h"

#ifdef HW1_1_V2
#include <tempo/tempo.h>

/**
 * @brief Stub application for compile check
 *
 */

// —— Hardware

class ArduinoClock : public tempo::Clock {
public:
    uint32_t now_ms() const override {
        return ::millis();
    }
};

class ArduinoStreamWriter : public tempo::StreamWriter {
public:
    size_t write(const char* data, size_t length) override {
        return Serial.write(data, length);
    }
};

// —— Events

class LoggingEmitted {
    uint32_t ms;
};

using Event = tempo::Events<LoggingEmitted>;
// —— Stage forward declarations

class BootStage;
class ObtainStage;

// —— Application alias (depends on stage list, not on stage definitions)

using App = tempo::Application<Event, BootStage, ObtainStage>;

// —— Stage definitions

class BootStage : public App::Stage, public tempo::UseLog<BootStage> {
public:
    static constexpr const char* LOG_TAG = "BootStage";

    const char* name() const override {
        return "INIT";
    }

    void on_tick(Conductor& conductor, uint32_t now_ms) override {
        if (!tempo::time_reached(now_ms, 3000)) {
            return;
        }

        log.info("BootStage: 3 seconds elapsed, requesting ObtainStage");
        conductor.request<ObtainStage>();
    }
};

class ObtainStage : public App::Stage {
public:
    const char* name() const override {
        return "MAIN";
    }
};

// —— Tasks

class LoggingTask : public App::StageScopedTask, public tempo::UseLog<LoggingTask> {
public:
    static constexpr const char* LOG_TAG = "LoggingTask";
    static constexpr uint32_t PERIOD_MS = 1000;

    LoggingTask() : App::StageScopedTask(PERIOD_MS, StageMaskType::of<ObtainStage>()) {}

    void on_tick(uint32_t now_ms) override {
        log.info("Time since boot: %dms", now_ms);
    }
};

// —— Application

ArduinoClock arduino_clock;
ArduinoStreamWriter arduino_stream_writer;

App app(arduino_clock, arduino_stream_writer);

BootStage boot_stage;
ObtainStage obtain_stage;

LoggingTask logging_task;

void setup() {
    Serial.begin(115200);
    app.add_task(logging_task);

    app.register_stage(boot_stage);
    app.register_stage(obtain_stage);
    app.start<BootStage>();
}

void loop() {
    app.tick();
}
#else
#include <v1/StateMachine.h>

static StateMachine statemachine;

void setup() {}

void loop() {
    statemachine.update();
}
#endif
