#include <Arduino.h>
#include <INA226.h>
#include <AP33772_PocketPD.h>
#include <Button.h>
#include <RotaryEncoder.h>
#include <U8g2lib.h>
#include <PocketPDPinOut.h>
#include <image.h>

#define ALARM_NUM                   0       //Timer 0
#define ALARM_IRQ                   TIMER_IRQ_0
#define DELAY                       100000     //In usecond

enum class State {BOOT, OBTAIN, CAPDISPLAY, NORMAL, MENU};



enum Supply_Mode {MODE_CV, MODE_CC};
enum Supply_Capability {NON_PPS, WITH_PPS};
enum Supply_Adjust_Mode {VOTLAGE_ADJUST, CURRENT_ADJUST};

#ifndef STATEMACHINE_H
#define STATEMACHINE_H

class StateMachine {
    public:
        StateMachine():
            ina226(0x40),
            u8g2(U8G2_R0, U8X8_PIN_NONE),
            state(State::BOOT),
            startTime(millis()), 
            bootInitialized(false), 
            obtainInitialized(false),
            displayCapInitialized(false),
            normalInitialized(false),
            button_encoder(pin_encoder_SW),
            button_output(pin_button_outputSW),
            button_selectVI(pin_button_selectVI),
            supply_mode(MODE_CV){};
            

        void update();
        void pressButton();
        void releaseButton();
        const char* getState();

    private:
        State state;
        INA226 ina226;
        AP33772 usbpd;
        static RotaryEncoder encoder; //"static" Make sure there is only 1 copy for all object, shared variable
        U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
        Button button_encoder;
        Button button_output;
        Button button_selectVI;

        unsigned long startTime;
        bool bootInitialized;
        bool obtainInitialized;
        bool displayCapInitialized;
        bool normalInitialized;
        bool buttonPressed;

        static bool timerFlag;

        int targetVoltage;      // Unit mV
        int targetCurrent;      // Unit mA
        int voltageIncrement;   // unit mV
        int currentIncrement;   // Unit mA
        float ina_current_ma;   // Unit
        float vbus_voltage;     // Unit mV
        int encoder_position;
        int encoder_newPos;
        Supply_Mode supply_mode;

        static constexpr long BOOT_TO_OBTAIN_TIMEOUT = 500;   // Timeout for BOOT to OBTAIN state in seconds
        static constexpr long OBTAIN_TO_CAPDISPLAY_TIMEOUT = 1500; // Timeout for OBTAIN to DISPLAYCAP state in seconds
        static constexpr long DISPLAYCCAP_TO_NORMAL_TIMEOUT = 3000; // Timeout for DISPLAYCAP to NORMAL state in seconds
        void componentInit();
        void handleBootState();
        void handleObtainState();
        void handleDisplayCapState();
        void handleNormalState();
        void handleMenuState();
        void transitionTo(State newState);
        void printBootingScreen();
        void printProfile();
        void printOLED_fixed();
        void updateOLED(float voltage, float current);

        static void encoderISR();
        static void timerISR();
        char buffer[10];
};


#endif //STATEMACHINE_H