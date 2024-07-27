//https://www.best-microcontroller-projects.com/rotary-encoder.html#Taming_Noisy_Rotary_Encoders

#include <Arduino.h>
#include <AP33772_PocketPD.h>
#include <U8g2lib.h>
#include <Wire.h>
// #include <neotimer.h>
#include <ezButton.h>
#include <INA226.h>
#include <RotaryEncoder.h>
#include <image.h>
#include <StateMachine.h>



StateMachine statemachine;




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
void encoder_read_update();

char buffer[10]; //Buffer for OLED

void setup()
{
}


void loop()
{
    statemachine.update();
//   // Initital state machine
 

//   // MUST call the loop() function first
//   output_Button.loop();
//   selectVI_Button.loop();
//   encoder_SW.loop();


//   //Appication with statemachine need to be here
//   /**
//    * State 1: Booting screen and init all parameter as well as start collecting PDOs
//    * State 3: Just normal loop
//    * State 4: Go into Menu Selection
//    */

//   if (mytimer_100ms.repeat()) //Reduce refresh rate of OLED
//   {
//     //Compensating for voltage drop accross 10mOhm and PMOS
//     ina_current_ma = abs(ina226.getCurrent_mA());
//     vbus_voltage = ina226.getBusVoltage_mV();
//     if(digitalRead(pin_output_Enable))
//       updateOLED(vbus_voltage /1000.0, ina_current_ma/1000);
//     else
//       updateOLED(usbpd.readVoltage() /1000.0, ina_current_ma/1000);
//   }

//   if (mytimer_500ms.repeat()) //Reduce refresh rate of OLED
//     {
//       updateSupplyMode();
//     }


//   process_buttons_press();
//   //Rotary check
//   process_request_voltage_current();

// }

// /**
//  * @brief process_buttons_press
//  * Check isPressed flag and process shortPress, longPress
//  * output_Button, selectVI_Button, and  encoder_SW
//  */
// void process_buttons_press()
// {
//   if(output_Button.isPressed())
//   {
//       Serial.print("Output button pressed \n");
//       digitalWrite(pin_output_Enable, !digitalRead(pin_output_Enable));
//   }

//   if(selectVI_Button.isPressed())
//   {
//     Serial.print("V-I button pressed \n");
//     //Complicated toggle - Can optimize
//     if(supply_adjust_mode == VOTLAGE_ADJUST)
//       supply_adjust_mode = CURRENT_ADJUST;
//     else
//       supply_adjust_mode = VOTLAGE_ADJUST;
//   }

//   //Button press
//   if(encoder_SW.isPressed())
//   {
//     if(supply_adjust_mode == VOTLAGE_ADJUST)
//     {
//       Serial.println("Changing voltage increment \n");
//       if(voltageIncrement == 200)
//         voltageIncrement = 20;
//       else if (voltageIncrement == 20)
//         voltageIncrement = 200;
//     }
//     else
//     {
//       Serial.println("Changing current increment \n");
//       if(currentIncrement == 200)
//         currentIncrement = 50;
//       else if (currentIncrement == 50)
//         currentIncrement = 200;
//     }
//   }
// }

// /**
//  * update_request_voltage_current
//  * @brief Read changes from encoder, compute request voltage/current
//  * Sent request to AP33772 to update
//  */
// void process_request_voltage_current()
// {
//   static int val;
//   if(val = (int8_t)encoder.getDirection())
//   {
//     if(supply_adjust_mode == VOTLAGE_ADJUST)
//     {
//       targetVoltage = targetVoltage + val*voltageIncrement;
//       Serial.println(targetVoltage);
      
//       if(usbpd.existPPS)
//       {
//         if((float)usbpd.getPPSMinVoltage(usbpd.getPPSIndex()) <= targetVoltage && (float)usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()) >= targetVoltage)
//           //usbpd.setVoltage(targetVoltage);
//           usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
//         else if (usbpd.getPPSMinVoltage(usbpd.getPPSIndex()) > targetVoltage)
//           targetVoltage = usbpd.getPPSMinVoltage(usbpd.getPPSIndex()); //No change
//         else if (usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()) < targetVoltage)
//           targetVoltage = usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()); //No change
//       }
//       else
//       { // PDOs only has profile between 5V and 20V
//         if(targetVoltage > 20000) targetVoltage = 20000;
//         else if (targetVoltage < 5000) targetVoltage = 5000;
//         usbpd.setVoltage(targetVoltage);
//       }
//     }

//     else
//     {
//       targetCurrent = targetCurrent + val*currentIncrement;
//       Serial.println(targetCurrent);
//       if(usbpd.existPPS)
//       {
//         if(targetCurrent < 1000)
//           targetCurrent = 1000; // Cap at 100mA minimum, no current update
//         else if(usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()) >= targetCurrent)
//           //usbpd.setMaxCurrent(targetCurrent);
//           usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
//         else if(usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()) < targetCurrent)
//           targetCurrent = usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()); //No change
//       }
//       else targetCurrent = usbpd.getMaxCurrent(); //Pull current base on current PDO
//       }
//   }
// }



// void updateSupplyMode()
// {
//   if(usbpd.existPPS && (targetVoltage >= (vbus_voltage + ina_current_ma*0.314 + 50)) && digitalRead(pin_output_Enable) ) //0.25Ohm for max round trip resistance + 0.02 Ohm for connector + 0.044 Ohm switch + 50mV margin
//     supply_mode = MODE_CC;
//   else
//     supply_mode = MODE_CV;  
// }

}