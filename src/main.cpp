//https://www.best-microcontroller-projects.com/rotary-encoder.html#Taming_Noisy_Rotary_Encoders

#include <Arduino.h>
#include <AP33772_PocketPD.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <neotimer.h>
#include <ezButton.h>
#include <INA226.h>
#include <RotaryEncoder.h>
#include <image.h>

#define output_SW_Button 10
#define outputSWPin 1



enum Supply_Mode {MODE_CV, MODE_CC};
enum Supply_Capability {NON_PPS, WITH_PPS};
enum Supply_Adjust_Mode {VOTLAGE_ADJUST, CURRENT_ADJUST};


/**
 * Function declare
 */
void encoderISR();


bool state = 0; 
int targetVoltage; // Unit mV
int targetCurrent; // Unit mA
int voltageIncrement; // unit mV
int currentIncrement; // Unit mA
float ina_current_ma; // Unit
float vbus_voltage; // Unit mV
int encoder_position;
int encoder_newPos;

#define switchPin_V  18       // button pin
#define pinA  19             // Rotary encoder Pin A //CLK
#define pinB  20            // Rotary encoder Pin B //DATA

#define outputSWPin 1
#define output_SW_Button 10
#define selectVI_SW_Button 11

ezButton encoder_SW(switchPin_V);
ezButton output_Button(output_SW_Button);
ezButton selectVI_Button(selectVI_SW_Button);


//Encoder variable
static uint8_t prevNextCode_V = 0;
static uint8_t prevNextCode_I = 0;
static uint16_t store_V=0;
static uint16_t store_I=0;
static Supply_Mode supply_mode = MODE_CV;
static Supply_Adjust_Mode supply_adjust_mode = VOTLAGE_ADJUST;

// https://github.com/olikraus/u8g2/issues/2289
// Do not pass SCL SDA into u8g2 constructor, or the I2C bus will break
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); //No rotation, no reset, SCL=5 SDA=4
INA226 ina226(0x40);
AP33772 usbpd;                          // Automatically wire to Wire0,
Neotimer mytimer_100ms = Neotimer(100); // Set timer's preset to 1s
Neotimer mytimer_500ms = Neotimer(500); // Set timer's preset to 1s
RotaryEncoder encoder(pinA, pinB, RotaryEncoder::LatchMode::FOUR3);

int8_t read_rotaryEncoder();
void printBootingScreen();
void printProfile();
void printOLED_fixed();
void updateSupplyMode();
void updateOLED(float voltage, float current);
void encoder_read_update();

char buffer[10]; //Buffer for OLED

/**
 * State Start here
 */
enum class State {BOOT, CAPABILITY, NORMAL, MENU};

class StateMachine {
public:
    StateMachine() 
        : state(State::BOOT), startTime(std::chrono::steady_clock::now()), bootInitialized(false), buttonPressed(false) {}

    void update() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        switch (state) {
            case State::BOOT:
                handleBootState();
                if (buttonPressed) {
                    transitionTo(State::NORMAL);
                } else if (elapsed >= BOOT_TO_CAPABILITY_TIMEOUT) {
                    transitionTo(State::CAPABILITY);
                }
                break;

            case State::CAPABILITY:
                handleCapabilityState();
                if (elapsed >= CAPABILITY_TO_NORMAL_TIMEOUT) {
                    transitionTo(State::NORMAL);
                }
                break;

            case State::NORMAL:
                handleNormalState();
                if (buttonPressed) {
                    transitionTo(State::MENU);
                }
                break;

            case State::MENU:
                handleMenuState();
                break;
        }
    }

    void pressButton() {
        buttonPressed = true;
    }

    void releaseButton() {
        buttonPressed = false;
    }

    std::string getState() const {
        switch (state) {
            case State::BOOT: return "BOOT";
            case State::CAPABILITY: return "CAPABILITY";
            case State::NORMAL: return "NORMAL";
            case State::MENU: return "MENU";
            default: return "UNKNOWN";
        }
    }
private:
    State state;
    std::chrono::steady_clock::time_point startTime;
    LED led;
    bool bootInitialized;
    bool buttonPressed;

    static constexpr int BOOT_TO_CAPABILITY_TIMEOUT = 5;   // Timeout for BOOT to CAPABILITY state in seconds
    static constexpr int CAPABILITY_TO_NORMAL_TIMEOUT = 10; // Timeout for CAPABILITY to NORMAL state in seconds

    void handleBootState() {
        if (!bootInitialized) {
            led.turnOn(); // Turn on the LED when entering the BOOT state
            bootInitialized = true; // Mark BOOT state as initialized
            std::cout << "Initialized BOOT state" << std::endl;
        }
        // Add additional BOOT state routines here
        std::cout << "Handling BOOT state" << std::endl;
    }

    void handleCapabilityState() {
        // Add CAPABILITY state routines here
        std::cout << "Handling CAPABILITY state" << std::endl;
    }

    void handleNormalState() {
        // Add NORMAL state routines here
        std::cout << "Handling NORMAL state" << std::endl;
    }

    void handleMenuState() {
        // Add MENU state routines here
        std::cout << "Handling MENU state" << std::endl;
    }

    void transitionTo(State newState) {
        if (newState == State::BOOT) {
            if (!bootInitialized) {
                led.turnOn(); // Turn on the LED when entering the BOOT state
                bootInitialized = true; // Mark BOOT state as initialized
                std::cout << "Entered BOOT state" << std::endl;
            }
        } else {
            if (state == State::BOOT) {
                led.turnOff(); // Turn off the LED when leaving the BOOT state
            }
        }

        std::cout << "Transitioning from " << getState() << " to " << (newState == State::BOOT ? "BOOT" :
                                                                     newState == State::CAPABILITY ? "CAPABILITY" :
                                                                     newState == State::NORMAL ? "NORMAL" :
                                                                     "MENU") << std::endl;
        state = newState;
        startTime = std::chrono::steady_clock::now(); // Reset the timer

        if (newState == State::BOOT) {
            bootInitialized = false; // Reset the initialization flag when entering BOOT
        }
        buttonPressed = false; // Reset button press flag when transitioning to another state
    }
};
/**
 * State End here
 */












void setup()
{
  Wire.setSDA(4);
  Wire.setSCL(5);
  
  //Wire.begin();
  u8g2.begin();

  // pinMode(switchPin_V, INPUT_PULLUP);
  // pinMode(pinA, INPUT_PULLUP);
  // pinMode(pinB, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pinA),encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinB),encoderISR, CHANGE);

  pinMode(output_SW_Button, INPUT_PULLUP);
  pinMode(selectVI_SW_Button, INPUT_PULLUP);

  pinMode(outputSWPin, OUTPUT); // Load Switch
  digitalWrite(outputSWPin, LOW);

  encoder_SW.setDebounceTime(50); // set debounce time to 50 milliseconds
  output_Button.setDebounceTime(50);
  selectVI_Button.setDebounceTime(50);

  Serial.begin(115200);
  
  if (!ina226.begin() )
  {
    while(1);
    {
      Serial.println("Could not connect to Current Sense. Fix and Reboot");
    }
  }
  ina226.setMaxCurrentShunt(6, 0.01); //Set max current to 6A with shunt resistance of 10mOhm

  printBootingScreen();
  delay(2000); // TODO, remove delay and make it skipable
  usbpd.begin(); // Start pulling the PDOs from power supply
  printProfile();
  delay(3000);

  targetVoltage = 5000; //Default start up voltage
  targetCurrent = 1000; //Default start up current
  voltageIncrement = 20; // 20mV
  currentIncrement = 50; // 50mA

  usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent); //Set wake up voltage. Default to 5V.
                                                               //Setting lower voltage will cause "Low voltage error" in Anker powerbank

  //delay(2000); //Random delay, optional
}

class StateMachine

void loop()
{
  // Initital state machine
  MachineState currentState = BOOT;

  //one example
  //https://digint.ch/tinyfsm/doc/usage.html
  switch (currentState)
  {
    case BOOT:
      /**
       * Booting screen and init all parameter as well as start collecting PDOs
       */
      break;
    case DISPLAY:
      /**
       * Capability display, can do 2 pages if later support QC
       */
      break;
    case NORMAL:
      /**
       * Normal loop to to check encoder, and update the target request
       */
      break;
    case MENU:
      /**
       * Reserve for MENU function, like changing the PDOs, or to QC
       */
      break;
    default:
      Serial.println("Encounter Error. Just entered default state.");

  }

  // MUST call the loop() function first
  output_Button.loop();
  selectVI_Button.loop();
  encoder_SW.loop();


  //Appication with statemachine need to be here
  /**
   * State 1: Booting screen and init all parameter as well as start collecting PDOs
   * State 3: Just normal loop
   * State 4: Go into Menu Selection
   */

  if (mytimer_100ms.repeat()) //Reduce refresh rate of OLED
  {
    //Compensating for voltage drop accross 10mOhm and PMOS
    ina_current_ma = abs(ina226.getCurrent_mA());
    vbus_voltage = ina226.getBusVoltage_mV();
    if(digitalRead(outputSWPin))
      updateOLED(vbus_voltage /1000.0, ina_current_ma/1000);
    else
      updateOLED(usbpd.readVoltage() /1000.0, ina_current_ma/1000);
  }

  if (mytimer_500ms.repeat()) //Reduce refresh rate of OLED
    {
      updateSupplyMode();
    }


  process_buttons_press();
  //Rotary check
  process_request_voltage_current();

}

/**
 * @brief process_buttons_press
 * Check isPressed flag and process shortPress, longPress
 * output_Button, selectVI_Button, and  encoder_SW
 */
void process_buttons_press()
{
  if(output_Button.isPressed())
  {
      Serial.print("Output button pressed \n");
      digitalWrite(outputSWPin, !digitalRead(outputSWPin));
  }

  if(selectVI_Button.isPressed())
  {
    Serial.print("V-I button pressed \n");
    //Complicated toggle - Can optimize
    if(supply_adjust_mode == VOTLAGE_ADJUST)
      supply_adjust_mode = CURRENT_ADJUST;
    else
      supply_adjust_mode = VOTLAGE_ADJUST;
  }

  //Button press
  if(encoder_SW.isPressed())
  {
    if(supply_adjust_mode == VOTLAGE_ADJUST)
    {
      Serial.println("Changing voltage increment \n");
      if(voltageIncrement == 200)
        voltageIncrement = 20;
      else if (voltageIncrement == 20)
        voltageIncrement = 200;
    }
    else
    {
      Serial.println("Changing current increment \n");
      if(currentIncrement == 200)
        currentIncrement = 50;
      else if (currentIncrement == 50)
        currentIncrement = 200;
    }
  }
}

/**
 * update_request_voltage_current
 * @brief Read changes from encoder, compute request voltage/current
 * Sent request to AP33772 to update
 */
void process_request_voltage_current()
{
  static int val;
  if(val = (int8_t)encoder.getDirection())
  {
    if(supply_adjust_mode == VOTLAGE_ADJUST)
    {
      targetVoltage = targetVoltage + val*voltageIncrement;
      Serial.println(targetVoltage);
      
      if(usbpd.existPPS)
      {
        if((float)usbpd.getPPSMinVoltage(usbpd.getPPSIndex()) <= targetVoltage && (float)usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()) >= targetVoltage)
          //usbpd.setVoltage(targetVoltage);
          usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
        else if (usbpd.getPPSMinVoltage(usbpd.getPPSIndex()) > targetVoltage)
          targetVoltage = usbpd.getPPSMinVoltage(usbpd.getPPSIndex()); //No change
        else if (usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()) < targetVoltage)
          targetVoltage = usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()); //No change
      }
      else
      { // PDOs only has profile between 5V and 20V
        if(targetVoltage > 20000) targetVoltage = 20000;
        else if (targetVoltage < 5000) targetVoltage = 5000;
        usbpd.setVoltage(targetVoltage);
      }
    }

    else
    {
      targetCurrent = targetCurrent + val*currentIncrement;
      Serial.println(targetCurrent);
      if(usbpd.existPPS)
      {
        if(targetCurrent < 1000)
          targetCurrent = 1000; // Cap at 100mA minimum, no current update
        else if(usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()) >= targetCurrent)
          //usbpd.setMaxCurrent(targetCurrent);
          usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
        else if(usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()) < targetCurrent)
          targetCurrent = usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()); //No change
      }
      else targetCurrent = usbpd.getMaxCurrent(); //Pull current base on current PDO
      }
  }
}



void updateSupplyMode()
{
  if(usbpd.existPPS && (targetVoltage >= (vbus_voltage + ina_current_ma*0.314 + 50)) && digitalRead(outputSWPin) ) //0.25Ohm for max round trip resistance + 0.02 Ohm for connector + 0.044 Ohm switch + 50mV margin
    supply_mode = MODE_CC;
  else
    supply_mode = MODE_CV;  
}


void printBootingScreen()
{
  u8g2.clearBuffer();
  u8g2.drawBitmap(0, 0, 128/8, 64, image_PocketPD_Logo);
  u8g2.sendBuffer();
}

void updateOLED(float voltage, float current)  
{
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  //Start-Fixed
  u8g2.setFont(u8g2_font_profont22_tr);
  u8g2.drawStr(1, 46, "A");
  u8g2.drawStr(1, 14, "V");
  if(usbpd.existPPS)
  {
    u8g2.drawXBM(95, 45, 11, 16, image_check_contour_bits);
  }
  else
  {
    u8g2.drawXBM(95, 45, 11, 16, image_cross_contour_bits);
  }
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(110, 58, "PPS");
  //End-Fixed
  u8g2.setFont(u8g2_font_profont22_tr);
  sprintf(buffer, "%.2f", voltage);
  u8g2.drawStr(75-u8g2.getStrWidth(buffer), 14, buffer); //Right adjust
  sprintf(buffer, "%.2f", current);
  u8g2.drawStr(75-u8g2.getStrWidth(buffer), 45, buffer); //Right adjust

  //https://github.com/olikraus/u8g2/discussions/2028
  u8g2.setFont(u8g2_font_profont15_tr);
  sprintf(buffer, "%d mV", targetVoltage);
  u8g2.drawStr(75-u8g2.getStrWidth(buffer), 27, buffer); //Right adjust
  sprintf(buffer, "%d mA", targetCurrent);
  u8g2.drawStr(75-u8g2.getStrWidth(buffer), 58, buffer); //Right adjust
  u8g2.setFont(u8g2_font_profont12_tr);
  if(supply_mode) //CV = 0, CC = 1
  {
    u8g2.drawStr(110, 13, "CV");
    u8g2.drawStr(110, 26, "CC");
    u8g2.setDrawColor(2);
    u8g2.drawBox(104, 16, 23, 12); // CC
  }
  else
  {
    u8g2.drawStr(110, 13, "CV");
    u8g2.drawStr(110, 26, "CC");
    u8g2.setDrawColor(2);
    u8g2.drawBox(104, 3, 23, 12); // CV
  }

  u8g2.sendBuffer();
}

/**
   * Print out PPS/PDO profiles
  */
void printProfile()
{
  int linelocation = 9;
  /**
   * Print out PPS if exist, else, print out PDOs
   * Missing: cannot display multiple PPS profile
  */
  u8g2.clearBuffer();

  if(usbpd.getNumPDO() == 0)
  {
    u8g2.setFontMode(1);
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(8, 34, "No Profile Detected");
    u8g2.sendBuffer();
    delay(3000);
    return;
  }

  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_profont11_tr);
  //Print PDOs onto OLED
  for(byte i = 0; i< usbpd.getNumPDO(); i++){
    if(i != usbpd.getPPSIndex()){
      linelocation = 9*(i+1);
      u8g2.setCursor(0, linelocation);
      u8g2.print("PDO: ");
      u8g2.print(usbpd.getPDOVoltage(i)/1000.0,0);
      u8g2.print("V @ ");
      u8g2.print(usbpd.getPDOMaxcurrent(i)/1000.0,0);
      u8g2.print("A");
    }
    else if(usbpd.existPPS){
      linelocation = 9*(i+1);
      u8g2.setCursor(0, linelocation);
      u8g2.print("PPS: ");
      u8g2.print(usbpd.getPPSMinVoltage(i)/1000.0,1);
      u8g2.print("V~");
      u8g2.print(usbpd.getPPSMaxVoltage(i)/1000.0,1);
      u8g2.print("V @ ");
      u8g2.print(usbpd.getPPSMaxCurrent(i)/1000.0,0);
      u8g2.print("A");
    }
  }
  u8g2.sendBuffer();
  
}

//Hardware improvement will reduce number of interrupt call
void encoderISR()
{
  encoder.tick(); //Just call tick() to check the state
}