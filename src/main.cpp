//https://www.best-microcontroller-projects.com/rotary-encoder.html#Taming_Noisy_Rotary_Encoders

#include <Arduino.h>
#include <AP33772_PocketPD.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <neotimer.h>
#include <ezButton.h>
#include <INA226.h>

#define output_SW_Button 10
#define outputSWPin 1

// https://github.com/olikraus/u8g2/issues/2289
// Do not pass SCL SDA into u8g2 constructor, or the I2C bus will break
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); //No rotation, no reset, SCL=5 SDA=4
INA226 ina226(0x40);
AP33772 usbpd;                          // Automatically wire to Wire0,
Neotimer mytimer_100ms = Neotimer(100); // Set timer's preset to 1s
Neotimer mytimer_500ms = Neotimer(500); // Set timer's preset to 1s

enum Supply_Mode {MODE_CV, MODE_CC};
enum Supply_Capability {NON_PPS, WITH_PPS};
enum Supply_Adjust_Mode {VOTLAGE_ADJUST, CURRENT_ADJUST};

bool state = 0; 
int targetVoltage; // Unit mV
int targetCurrent; // Unit mA
int voltageIncrement; // unit mV
int currentIncrement; // Unit mA
float ina_current_ma; // Unit
float vbus_voltage; // Unit mV

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

int8_t read_rotaryEncoder();
void printBootingScreen();
void printProfile();
void printOLED_fixed();
void updateSupplyMode();
void updateOLED(float voltage, float current);

static const unsigned char image_check_contour_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x0a,0x04,0x11,0x8a,0x08,0x51,0x04,0x22,0x02,0x04,0x01,0x88,0x00,0x50,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_cross_contour_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x01,0x8a,0x02,0x51,0x04,0x22,0x02,0x04,0x01,0x88,0x00,0x04,0x01,0x22,0x02,0x51,0x04,0x8a,0x02,0x04,0x01,0x00,0x00,0x00,0x00};
// 'PocketPD_Logo', 128x64px
const unsigned char image_PocketPD_Logo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0x80, 0x07, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x9f, 0xf0, 0x3f, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x83, 0xff, 0xfe, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x3f, 0xf0, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x03, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0xfe, 0x01, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0xff, 0xc1, 0xff, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x03, 0x00, 0x0c, 0x00, 0x07, 0xff, 0xc1, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x07, 0x80, 0x0c, 0x00, 0x07, 0x83, 0xe1, 0xe0, 0x3f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x07, 0x80, 0x0c, 0x00, 0x07, 0x81, 0xe1, 0xe0, 0x0f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x03, 0x00, 0x0c, 0x00, 0x07, 0x80, 0xf1, 0xe0, 0x07, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0xf1, 0xe0, 0x07, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0xf1, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x81, 0xe1, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x81, 0xe1, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0xff, 0xe1, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0xff, 0xc1, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0xff, 0x01, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0xf8, 0x01, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xe0, 0x03, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xe0, 0x07, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xe0, 0x07, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xe0, 0x0f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xe0, 0x3f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xff, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x80, 0x01, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xfc, 0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7f, 0xc0, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x07, 0xf8, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

char buffer[10]; //Buffer for OLED


void setup()
{
  Wire.setSDA(4);
  Wire.setSCL(5);
  
  //Wire.begin();
  u8g2.begin();

  pinMode(switchPin_V, INPUT_PULLUP);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);

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
  delay(2000);
  usbpd.begin(); // Start pulling the PDOs from power supply
  printProfile();

  targetVoltage = 5000; //Start with just 5V
  targetCurrent = 3000; //Maxout standard current
  voltageIncrement = 20; // 20mV
  currentIncrement = 50; // 50mA

  usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent); //Set wake up voltage. Default to 5V.
                                                               //Setting lower voltage will cause "Low voltage error" in Anker powerbank

  //delay(2000); //Random delay, optional
}

void loop()
{
  static int8_t c,val;

  // MUST call the loop() function first
  encoder_SW.loop(); 
  output_Button.loop();
  selectVI_Button.loop();

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


  ////Need to make seperate function
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
    Serial.println("Changing voltage increment \n");
    if(voltageIncrement == 200)
      voltageIncrement = 20;
    else if (voltageIncrement == 20)
      voltageIncrement = 200;
  }

  //Rotary check
  if(val = read_rotaryEncoder())
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

/**
 * @brief Read voltage rotary encoder. Supress noise using state transision check
 * @returns A valid CW return 1 or valid CCW returns -1, invalid returns 0.
*/
int8_t read_rotaryEncoder() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  prevNextCode_V <<= 2;
  if (digitalRead(pinB)) prevNextCode_V |= 0x02;
  if (digitalRead(pinA)) prevNextCode_V |= 0x01;
  prevNextCode_V &= 0x0f;

   // If valid then store_V as 16 bit data.
   if  (rot_enc_table[prevNextCode_V] ) {
      store_V <<= 4;
      store_V |= prevNextCode_V;
      if ((store_V&0xff)==0x2b) return 1;
      if ((store_V&0xff)==0x17) return -1;
   }
   return 0;
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
    u8g2.drawXBM(86, 45, 11, 16, image_check_contour_bits);
  }
  else
  {
    u8g2.drawXBM(86, 45, 11, 16, image_cross_contour_bits);
  }
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(102, 58, "PPS");
  //End-Fixed
  u8g2.setFont(u8g2_font_profont22_tr);
  sprintf(buffer, "%.2f", voltage);
  u8g2.drawStr(80-u8g2.getStrWidth(buffer), 14, buffer); //Right adjust
  sprintf(buffer, "%.2f", current);
  u8g2.drawStr(80-u8g2.getStrWidth(buffer), 45, buffer); //Right adjust

  //https://github.com/olikraus/u8g2/discussions/2028
  u8g2.setFont(u8g2_font_profont15_tr);
  sprintf(buffer, "%d", targetVoltage);
  u8g2.drawStr(80-u8g2.getStrWidth(buffer), 27, buffer); //Right adjust
  sprintf(buffer, "%d", targetCurrent);
  u8g2.drawStr(80-u8g2.getStrWidth(buffer), 58, buffer); //Right adjust
  u8g2.setFont(u8g2_font_profont12_tr);
  if(supply_mode) //CV = 0, CC = 1
  {
    u8g2.drawStr(101, 13, "CV");
    u8g2.drawStr(101, 26, "CC");
    u8g2.setDrawColor(2);
    u8g2.drawBox(95, 16, 23, 12); // CC
  }
  else
  {
    u8g2.drawStr(101, 13, "CV");
    u8g2.drawStr(101, 26, "CC");
    u8g2.setDrawColor(2);
    u8g2.drawBox(95, 3, 23, 12); // CV
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
  delay(3000);
}