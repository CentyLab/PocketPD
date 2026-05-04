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
#include "v2/hal/ez_button_input.h"
#include "v2/hal/rotary_encoder_input.h"
#include "v2/hal/u8g2_display.h"
#include "v2/stages/boot_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/obtain_stage.h"
#include "v2/stages/pdo_picker_stage.h"
#include "v2/tasks/button_task.h"
#include "v2/tasks/encoder_task.h"
#include <AP33772.h>

namespace {

    pocketpd::ArduinoClock arduino_clock;
    pocketpd::ArduinoStreamWriter arduino_stream_writer;
    pocketpd::App app(arduino_clock, arduino_stream_writer);

    // —— Hardware adapters

    ArduinoTwoWireDevice ap33772_i2c{Wire, ap33772::ADDRESS};
    pocketpd::Ap33772PdSink pd_sink{ap33772_i2c, ::delay};

    pocketpd::U8g2Display u8g2_display;

    pocketpd::EzButtonInput encoder_button{pin_encoder_SW};
    pocketpd::EzButtonInput output_button{pin_button_outputSW};
    pocketpd::EzButtonInput select_vi_button{pin_button_selectVI};
    pocketpd::RotaryEncoderInput encoder{pin_encoder_A, pin_encoder_B};

    // —— Stages

    pocketpd::BootStage boot_stage(u8g2_display);
    pocketpd::ObtainStage obtain_stage(pd_sink);
    pocketpd::PdoPickerStage pdo_picker_stage(u8g2_display, pd_sink);
    pocketpd::NormalStage normal_stage;

    // —— Tasks

    pocketpd::ButtonTask button_task(encoder_button, select_vi_button, output_button);
    pocketpd::EncoderTask encoder_task(encoder);

} // namespace

void setup() {
    Serial.begin(115200);

    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);
    Wire.begin();

    u8g2_display.begin();
    encoder.begin();

    app.register_stage(boot_stage);
    app.register_stage(obtain_stage);
    app.register_stage(pdo_picker_stage);
    app.register_stage(normal_stage);

    app.add_task(button_task);
    app.add_task(encoder_task);

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
