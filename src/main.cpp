#include <AP33772.h>
#include <Arduino.h>
#include <ArduinoTwoWireDevice.h>
#include <INA226.h>
#include <PocketPDPinOut.h>
#include <Wire.h>
#include <tempo/tempo.h>

#include "v2/app.h"
#include "v2/hal/ap33772_pd_sink.h"
#include "v2/hal/ap33772_supply_voltage_source.h"
#include "v2/hal/adc_supply_voltage_source.h"
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

namespace pocketpd {

    

    // —— Hardware adapters

    ArduinoClock arduino_clock;
    ArduinoStreamWriter arduino_stream_writer;
    ArduinoStreamReader arduino_stream_reader;

    App app(arduino_clock, arduino_stream_writer);

    ArduinoTwoWireDevice i2c_device_ap33772{Wire, ap33772::ADDRESS};
    Ap33772PdSink pd_sink{i2c_device_ap33772, ::delay};

    INA226 ina226_driver{INA226_I2C_ADDR};
    Ina226PowerMonitor power_monitor{ina226_driver};

#if HW_VERSION_MAJOR == 1 && HW_VERSION_MINOR >= 3
    AdcSupplyVoltageSource supply_voltage_source{pin_VSENSE};
#else
    Ap33772SupplyVoltageSource supply_voltage_source{pd_sink};
#endif

    U8g2Display u8g2_display;

    ArduinoOutputGate output_gate{pin_output_Enable};

    EzButtonInput encoder_button{pin_encoder_SW};
    EzButtonInput r_button{pin_button_outputSW};
    EzButtonInput l_button{pin_button_selectVI};
    RotaryEncoderInput encoder{pin_encoder_A, pin_encoder_B};

    // —— Stages

    BootStage boot_stage(u8g2_display);
    ObtainStage obtain_stage(pd_sink);
    ProfilePickerStage profile_picker_stage(u8g2_display, pd_sink);
    NormalStage normal_stage(u8g2_display, pd_sink, output_gate);
    EnergyStage energy_stage(u8g2_display, output_gate);

    // —— Tasks

    ButtonTask button_task(encoder_button, l_button, r_button);
    EncoderTask encoder_task(encoder);
    SensorTask sensor_task{power_monitor, supply_voltage_source};
    EnergyTask energy_task{output_gate};
    CommandTask command_task{arduino_stream_reader, arduino_stream_writer};

} // namespace pocketpd

void setup() {
    Serial.begin(115200);

    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);
    Wire.begin();

    using namespace pocketpd;

    ina226_driver.begin();
    power_monitor.begin();
    supply_voltage_source.begin();
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

    app.start<BootStage>();
}

void loop() {
    pocketpd::app.tick();
}
