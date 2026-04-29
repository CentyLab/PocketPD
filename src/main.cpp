#include <Arduino.h>

#include <cmath>

#ifdef HW1_1_V2
#include <tempo/tempo.h>

/**
 * @brief Stub application for compile check
 *
 */

// Hardware

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

// Events

class LoggingEmitted {};
using Event = tempo::Events<LoggingEmitted>;

// Stages

enum class StageId : uint8_t {
    INIT,
    MAIN,
    ERROR,
    COUNT_,
};

// Tasks

class LoggingTask : public tempo::BackgroundTask<StageId, Event>,
                    public tempo::UseLog<LoggingTask> {
public:
    static constexpr const char* LOG_TAG = "LoggingTask";
    LoggingTask() : tempo::BackgroundTask<StageId, Event>(1000) {}

    void on_tick(uint32_t now_ms) override {
        log.info("Time since boot: %dms", now_ms);
    }
};

// Application
using App = tempo::Application<StageId, Event>;

ArduinoClock arduino_clock;
ArduinoStreamWriter arduino_stream_writer;

App app(arduino_clock, arduino_stream_writer);

LoggingTask logging_task;

void setup() {

    Serial.begin(115200);
    app.add_task(logging_task);
    app.start(StageId::MAIN);
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
