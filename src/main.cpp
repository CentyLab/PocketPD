#include <Arduino.h>

#ifdef HW1_3_V2
#include <PocketPDPinOut.h>
#include <Wire.h>
#include <tempo/tempo.h>

#include "v2/app.h"
#include "v2/hal/arduino_clock.h"
#include "v2/hal/arduino_stream_writer.h"
#include "v2/hal/u8g2_display.h"
#include "v2/stages/boot_stage.h"

namespace {

    pocketpd::ArduinoClock arduino_clock;
    pocketpd::ArduinoStreamWriter arduino_stream_writer;

    pocketpd::U8g2Display u8g2_display;

    pocketpd::App app(arduino_clock, arduino_stream_writer);
    pocketpd::BootStage boot_stage(u8g2_display);

} // namespace

void setup() {
    Serial.begin(115200);

    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);
    Wire.begin();

    u8g2_display.begin();

    app.register_stage(boot_stage);
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
