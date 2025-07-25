#include <Arduino.h>
#include <INA226.h>
#include <AP33772_PocketPD.h>
#include <Button.h>
#include <RotaryEncoder.h>
#include <U8g2lib.h>
#include <PocketPDPinOut.h>
#include <image.h>
#include <Menu.h>
#include <EEPROMHandler.hpp>

#define ALARM_NUM0 0 // Timer 0
#define ALARM_IRQ0 TIMER_IRQ_0
#define DELAY0 33000 // In usecond , 33ms

#define ALARM_NUM1 1 // Timer 1
#define ALARM_IRQ1 TIMER_IRQ_1
#define DELAY1 500000 // In usecond , 500ms, used for cursor

enum class State
{
    BOOT,
    OBTAIN,
    CAPDISPLAY,
    NORMAL_PPS,
    NORMAL_PDO,
    NORMAL_QC,
    MENU
};
enum Supply_Profile
{
    PROFILE_PPS,
    PROFILE_PDO,
    PROFILE_QC
};
enum Supply_Mode
{
    MODE_CV,
    MODE_CC
};
enum Supply_Capability
{
    NON_PPS,
    WITH_PPS
};
enum Supply_Adjust_Mode
{
    VOTLAGE_ADJUST,
    CURRENT_ADJUST
};

#ifndef STATEMACHINE_H
#define STATEMACHINE_H

class StateMachine
{
public:
    StateMachine() : ina226(0x40),
                     u8g2(U8G2_R0, U8X8_PIN_NONE),
                     state(State::BOOT),
                     startTime(millis()),
                     bootInitialized(false),
                     obtainInitialized(false),
                     displayCapInitialized(false),
                     normalPPSInitialized(false),
                     normalPDOInitialized(false),
                     normalQCInitialized(false),
                     button_encoder(pin_encoder_SW),
                     button_output(pin_button_outputSW),
                     button_selectVI(pin_button_selectVI),
                     supply_mode(MODE_CV),
                     voltageIncrement{20, 100, 1000},
                     currentIncrement{50, 200},
                     menu(&u8g2, &usbpd, &encoder, &button_encoder, &button_output, &button_selectVI),
                     eepromHandler(),
                     forceSave(false),
                     saveStamp(0) {};

    void update();
    const char *getState();

private:
    State state;
    INA226 ina226;
    AP33772 usbpd;
    static RotaryEncoder encoder; //"static" Make sure there is only 1 copy for all object, shared variable
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
    Button button_encoder;
    Button button_output;
    Button button_selectVI;
    Menu menu;
    EEPROMHandler eepromHandler;
    bool forceSave;
    uint32_t saveStamp;

    unsigned long startTime;
    bool bootInitialized;
    bool obtainInitialized;
    bool displayCapInitialized;
    bool normalPPSInitialized;
    bool normalPDOInitialized;
    bool normalQCInitialized;
    bool buttonPressed;

    static bool timerFlag0;
    static bool timerFlag1;

    int targetVoltage;         // Unit mV
    int targetCurrent;         // Unit mA
    int voltageIncrementIndex; // unit mV
    int currentIncrementIndex; // Unit mA
    int voltageIncrement[3];
    int currentIncrement[2];
    float ina_current_ma;  // Unit
    float vbus_voltage_mv; // Unit mV
    int encoder_position;
    int encoder_newPos;
    Supply_Mode supply_mode;
    Supply_Adjust_Mode supply_adjust_mode;

    static constexpr long BOOT_TO_OBTAIN_TIMEOUT = 500;         // Timeout for BOOT to OBTAIN state in seconds
    static constexpr long OBTAIN_TO_CAPDISPLAY_TIMEOUT = 1500;  // Timeout for OBTAIN to DISPLAYCAP state in seconds
    static constexpr long DISPLAYCCAP_TO_NORMAL_TIMEOUT = 3000; // Timeout for DISPLAYCAP to NORMAL state in seconds
    static constexpr int voltage_cursor_position[] = {40, 33, 26};
    static constexpr int current_cursor_position[] = {41, 34};
    void componentInit();

    void handleBootState();
    void handleObtainState();
    void handleDisplayCapState();
    void handleNormalPPSState();
    void handleNormalPDOState();
    void handleNormalQCState();
    void handleMenuState();

    void transitionTo(State newState);
    void printBootingScreen();
    void printProfile();
    void printOLED_fixed();
    void updateOLED(float voltage, float current, uint8_t requestEN);
    void update_supply_mode();

    void process_request_voltage_current();
    static void encoderISR();
    static void timerISR0(); // 100ms
    static void timerISR1(); // 1s
    char buffer[10];

    int counter_gif = 0; //Count up to 27

    void handleInitialMode();
    bool saveSettingsToEEPROM();
    bool loadSettingsFromEEPROM(bool vc);
    void updateSaveStamp();
};

#endif // STATEMACHINE_H