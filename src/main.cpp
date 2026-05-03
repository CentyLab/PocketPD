#include <Arduino.h>

#ifdef HW1_3_V2
#include <ArduinoTwoWireDevice.h>
#include <PocketPDPinOut.h>
#include <Wire.h>
#include <tempo/tempo.h>

#include "v2/app.h"
#include "v2/hal/ap33772_pd_sink.h"
#include "v2/hal/arduino_clock.h"
#include "v2/hal/arduino_stream_writer.h"
#include "v2/hal/u8g2_display.h"
#include "v2/stages/boot_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/obtain_stage.h"
#include "v2/stages/pdo_picker_stage.h"
#include <AP33772.h>

namespace {

    pocketpd::ArduinoClock arduino_clock;
    pocketpd::ArduinoStreamWriter arduino_stream_writer;
    pocketpd::App app(arduino_clock, arduino_stream_writer);

    // —— Hardware adapters

    ArduinoTwoWireDevice ap33772_i2c{Wire, ap33772::ADDRESS};
    pocketpd::Ap33772PdSink pd_sink{ap33772_i2c, ::delay};

    pocketpd::U8g2Display u8g2_display;

    // —— Stages

    pocketpd::BootStage boot_stage(u8g2_display);
    pocketpd::ObtainStage obtain_stage(pd_sink, app.task_publisher());
    pocketpd::PdoPickerStage pdo_picker_stage(u8g2_display, pd_sink);
    pocketpd::NormalStage normal_stage;

} // namespace

void setup() {
    Serial.begin(115200);

    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);
    Wire.begin();

    u8g2_display.begin();

    app.register_stage(boot_stage);
    app.register_stage(obtain_stage);
    app.register_stage(pdo_picker_stage);
    app.register_stage(normal_stage);

    app.start<pocketpd::BootStage>();
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
