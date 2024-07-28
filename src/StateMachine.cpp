#include <StateMachine.h>

//Static object require separate implementation
RotaryEncoder StateMachine::encoder(pin_encoder_A, pin_encoder_B, RotaryEncoder::LatchMode::FOUR3);


void StateMachine::update()
{
    auto now = millis();
        auto elapsed = now - startTime;

        switch (state) {
            case State::BOOT:
                handleBootState();
                if (elapsed >= BOOT_TO_OBTAIN_TIMEOUT)
                    transitionTo(State::OBTAIN);
                break;

            case State::OBTAIN:
                handleObtainState();
                if (buttonPressed)
                    transitionTo(State::NORMAL);
                else if(elapsed >= OBTAIN_TO_CAPDISPLAY_TIMEOUT)
                    transitionTo(State::CAPDISPLAY);
                break;

            case State::CAPDISPLAY:
                handleDisplayCapState();
                if (elapsed >= DISPLAYCCAP_TO_NORMAL_TIMEOUT)
                    transitionTo(State::NORMAL);
                break;

            case State::NORMAL:
                handleNormalState();
                if (buttonPressed)
                    transitionTo(State::MENU);
                break;

            case State::MENU:
                handleMenuState();
                break;
        }
}


void StateMachine::pressButton(){};
void StateMachine::releaseButton(){};

const char* StateMachine::getState()
{
    switch (state) {
        case State::BOOT: return "BOOT";
        case State::OBTAIN: return "OBTAIN";
        case State::CAPDISPLAY: return "CAPABILITY";
        case State::NORMAL: return "NORMAL";
        case State::MENU: return "MENU";
        default: return "UNKNOWN";
}
}


void StateMachine::componentInit()
{
    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);
    

    attachInterrupt(digitalPinToInterrupt(pin_encoder_A),encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pin_encoder_B),encoderISR, CHANGE);

    pinMode(pin_button_outputSW, INPUT_PULLUP);
    pinMode(pin_button_selectVI, INPUT_PULLUP);

    pinMode(pin_output_Enable, OUTPUT); // Load Switch
    digitalWrite(pin_output_Enable, LOW);

    button_encoder.setDebounceTime(50); // set debounce time to 50 milliseconds
    button_output.setDebounceTime(50);
    button_selectVI.setDebounceTime(50);

    u8g2.begin();
    ina226.begin();
    ina226.setMaxCurrentShunt(6, 0.01);

    targetVoltage = 5000; //Default start up voltage
    targetCurrent = 1000; //Default start up current
    voltageIncrement = 20; // 20mV
    currentIncrement = 50; // 50mA
}

//Only need to declare static in header files
void StateMachine::encoderISR()
{
    encoder.tick();
}

void StateMachine::handleBootState()
{
    if (!bootInitialized) 
        {
          //* BEGIN Only run once when entering the state */
            //led.turnOn(); // Turn on the LED when entering the BOOT state
            componentInit();
            printBootingScreen();
            // ADD Check if connected to PC here
          //* END Only run once when entering the state */
            bootInitialized = true; // Mark BOOT state as initialized
            Serial.println( "Initialized BOOT state" );
        }
        //* BEGIN state routine */
        // Add additional BOOT state routines here


        //* END state routine */
        Serial.println( "Handling BOOT state" );
}

void StateMachine::handleObtainState()
{
    if (!obtainInitialized) 
        {
          //* BEGIN Only run once when entering the state */
            usbpd.begin(); // Start pulling the PDOs from power supply
            // ADD check QC3.0 code here
          //* END Only run once when entering the state */
            bootInitialized = true; // Mark BOOT state as initialized
            Serial.println( "Initialized BOOT state" );
        }
        //* BEGIN state routine */
        // Add additional BOOT state routines here


        //* END state routine */
        Serial.println( "Handling BOOT state" );
}

void StateMachine::handleDisplayCapState()
{
    if(!displayCapInitialized)
      {
        //* BEGIN Only run once when entering the state */
            //led.turnOn(); // Turn on the LED when entering the BOOT state
            printProfile();
            usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
        

          //* END Only run once when entering the state */
          displayCapInitialized = true;
      }
        // Add CAPABILITY state routines here
        Serial.println( "Handling CAPABILITY state" );
}

void StateMachine::handleNormalState()
{
    // Add NORMAL state routines here
    Serial.println( "Handling NORMAL state" );
}

void StateMachine::handleMenuState(){
    // Add MENU state routines here
    Serial.println( "Handling MENU state" );
}

void StateMachine::transitionTo(State newState)
{
    if (newState == State::BOOT) 
        {
            if (!bootInitialized) 
            {
                //led.turnOn(); // Turn on the LED when entering the BOOT state
                bootInitialized = true; // Mark BOOT state as initialized
                Serial.println("Entered BOOT state");
            }
        } else {
            if (state == State::BOOT) {
                //led.turnOff(); // Turn off the LED when leaving the BOOT state
            }
        }
        
        //Transitioning from BOOT to CAPABILITY
        Serial.print("Transitioning from ");
        Serial.print(getState());
        Serial.print("to ");
        Serial.println((newState == State::BOOT ? "BOOT" :newState == State::CAPDISPLAY ? "CAPABILITY" :newState == State::NORMAL ? "NORMAL" :"MENU"));

        state = newState;
        startTime = millis();

        if (newState == State::BOOT) {
            bootInitialized = false; // Reset the initialization flag when entering BOOT
        }
        buttonPressed = false; // Reset button press flag when transitioning to another state
}


void StateMachine::printBootingScreen()
{
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 128/8, 64, image_PocketPD_Logo);
    u8g2.sendBuffer();
}

// void StateMachine::updateOLED()
// {
//     u8g2.clearBuffer();
//     u8g2.setFontMode(1);
//     u8g2.setBitmapMode(1);
//     //Start-Fixed
//     u8g2.setFont(u8g2_font_profont22_tr);
//     u8g2.drawStr(1, 46, "A");
//     u8g2.drawStr(1, 14, "V");
//     if(usbpd.existPPS)
//     {
//         u8g2.drawXBM(95, 45, 11, 16, image_check_contour_bits);
//     }
//     else
//     {
//         u8g2.drawXBM(95, 45, 11, 16, image_cross_contour_bits);
//     }
//     u8g2.setFont(u8g2_font_profont12_tr);
//     u8g2.drawStr(110, 58, "PPS");
//     //End-Fixed
//     u8g2.setFont(u8g2_font_profont22_tr);
//     sprintf(buffer, "%.2f", voltage);
//     u8g2.drawStr(75-u8g2.getStrWidth(buffer), 14, buffer); //Right adjust
//     sprintf(buffer, "%.2f", current);
//     u8g2.drawStr(75-u8g2.getStrWidth(buffer), 45, buffer); //Right adjust

//     //https://github.com/olikraus/u8g2/discussions/2028
//     u8g2.setFont(u8g2_font_profont15_tr);
//     sprintf(buffer, "%d mV", targetVoltage);
//     u8g2.drawStr(75-u8g2.getStrWidth(buffer), 27, buffer); //Right adjust
//     sprintf(buffer, "%d mA", targetCurrent);
//     u8g2.drawStr(75-u8g2.getStrWidth(buffer), 58, buffer); //Right adjust
//     u8g2.setFont(u8g2_font_profont12_tr);
//     if(supply_mode) //CV = 0, CC = 1
//     {
//         u8g2.drawStr(110, 13, "CV");
//         u8g2.drawStr(110, 26, "CC");
//         u8g2.setDrawColor(2);
//         u8g2.drawBox(104, 16, 23, 12); // CC
//     }
//     else
//     {
//         u8g2.drawStr(110, 13, "CV");
//         u8g2.drawStr(110, 26, "CC");
//         u8g2.setDrawColor(2);
//         u8g2.drawBox(104, 3, 23, 12); // CV
//     }

//     u8g2.sendBuffer();
// }

/**
   * Print out PPS/PDO profiles
  */
void StateMachine::printProfile()
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