#include <AP33772.h>
#include <Arduino.h>
#include <ArduinoTwoWireDevice.h>
#include <INA226.h>
#include <PocketPDPinOut.h>
#include <Wire.h>
#include <tempo/tempo.h>

#include "v2/app.h"
#include "v2/hal/ap33772_pd_sink.h"
#include "v2/hal/arduino_clock.h"
#include "v2/hal/arduino_output_gate.h"
#include "v2/hal/arduino_stream_reader.h"
#include "v2/hal/arduino_stream_writer.h"
#include "v2/hal/ez_button_input.h"
#include "v2/hal/ina226_power_monitor.h"
#include "v2/hal/rotary_encoder_input.h"
#include "v2/hal/u8g2_display.h"
#include "v2/tasks/button_task.h"
#include "v2/tasks/command_task.h"
#include "v2/tasks/encoder_task.h"
#include "v2/tasks/energy_task.h"
#include "v2/tasks/sensor_task.h"

namespace {

    // —— Hardware adapters

    ArduinoTwoWireDevice i2c_device_ap33772{Wire, ap33772::ADDRESS};
    pocketpd::Ap33772PdSink pd_sink{i2c_device_ap33772, ::delay};

    INA226 ina226_driver{pocketpd::INA226_I2C_ADDR};
    pocketpd::Ina226PowerMonitor power_monitor{ina226_driver};

    pocketpd::U8g2Display u8g2_display;

    pocketpd::ArduinoOutputGate output_gate{pin_output_Enable};

    pocketpd::EzButtonInput encoder_button{pin_encoder_SW};
    pocketpd::EzButtonInput r_button{pin_button_outputSW};
    pocketpd::EzButtonInput l_button{pin_button_selectVI};
    pocketpd::RotaryEncoderInput encoder{pin_encoder_A, pin_encoder_B};

    pocketpd::ArduinoClock arduino_clock;
    pocketpd::ArduinoStreamWriter arduino_stream_writer;
    pocketpd::ArduinoStreamReader arduino_stream_reader;

    pocketpd::App app(arduino_clock, arduino_stream_writer);

    // —— Stages

    pocketpd::BootStage boot_stage(u8g2_display);
    pocketpd::ObtainStage obtain_stage(pd_sink);
    pocketpd::ProfilePickerStage profile_picker_stage(u8g2_display, pd_sink);
    pocketpd::NormalStage normal_stage(u8g2_display, pd_sink, output_gate);
    pocketpd::EnergyStage energy_stage(u8g2_display, output_gate);

    // —— Tasks

    pocketpd::ButtonTask button_task(encoder_button, l_button, r_button);
    pocketpd::EncoderTask encoder_task(encoder);
    pocketpd::SensorTask sensor_task{power_monitor};
    pocketpd::EnergyTask energy_task{output_gate};
    pocketpd::CommandTask command_task{arduino_stream_reader, arduino_stream_writer};

} // namespace

void setup() {
    Serial.begin(115200);

    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);
    Wire.begin();

    ina226_driver.begin();
    power_monitor.begin();
    u8g2_display.begin();
    output_gate.begin();
    encoder.begin();

    app.register_stage(boot_stage);
    app.register_stage(obtain_stage);
    app.register_stage(profile_picker_stage);
    app.register_stage(normal_stage);
    app.register_stage(energy_stage);

    app.add_task(button_task);
    app.add_task(encoder_task);
    app.add_task(sensor_task);
    app.add_task(energy_task);
    app.add_task(command_task);

    app.start<pocketpd::BootStage>();
}

void loop() {
    app.tick();
}
