#include <StateMachine.h>

//Static object require separate implementation
RotaryEncoder StateMachine::encoder(pin_encoder_A, pin_encoder_B, RotaryEncoder::LatchMode::FOUR3);
bool StateMachine::timerFlag0 = false; //Need to initilize Static variable
bool StateMachine::timerFlag1 = false; //Need to initilize Static variable


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
                if ((button_encoder.isButtonPressed() 
                    | button_output.isButtonPressed() 
                    | button_selectVI.isButtonPressed())  
                        == 1) //Short press
                    transitionTo(State::NORMAL_PPS);
                else if(elapsed >= OBTAIN_TO_CAPDISPLAY_TIMEOUT)
                    transitionTo(State::CAPDISPLAY);
                break;

            case State::CAPDISPLAY:
                handleDisplayCapState();
                if ((button_encoder.isButtonPressed() 
                    | button_output.isButtonPressed() 
                    | button_selectVI.isButtonPressed())  
                        == 1) //Short press
                    transitionTo(State::NORMAL_PPS);
                else if (elapsed >= DISPLAYCCAP_TO_NORMAL_TIMEOUT)
                    transitionTo(State::NORMAL_PPS);
                break;

            case State::NORMAL_PPS:
                handleNormalPPSState();
                if (button_selectVI.longPressedFlag) //Long press isButtonPressed() is called inside the state
                {
                    button_selectVI.clearLongPressedFlag();
                    transitionTo(State::MENU);
                }  
                break;
            case State::NORMAL_PDO:
                handleNormalPDOState();
                button_selectVI.isButtonPressed(); //Call to update long press flag
                if (button_selectVI.longPressedFlag) //Long press isButtonPressed() is called inside the state
                {
                    button_selectVI.clearLongPressedFlag();
                    transitionTo(State::MENU);
                }  
                break;
            case State::NORMAL_QC:
                handleNormalQCState();
                if (button_selectVI.longPressedFlag) //Long press isButtonPressed() is called inside the state
                {
                    button_selectVI.clearLongPressedFlag();
                    transitionTo(State::MENU);
                }  
                break;

            case State::MENU:
                handleMenuState();
                
                button_encoder.isButtonPressed(); 
                button_output.isButtonPressed() ;
                button_selectVI.isButtonPressed();

                if (button_selectVI.longPressedFlag) //Long press
                {
                    button_selectVI.clearLongPressedFlag();
                    transitionTo(State::NORMAL_PPS);
                }
                if (button_encoder.longPressedFlag) //Long press
                {
                    button_encoder.clearLongPressedFlag();
                    if(menu.menuPosition == usbpd.getPPSIndex())
                        transitionTo(State::NORMAL_PPS);
                    else
                        transitionTo(State::NORMAL_PDO);
                }
                break;
        }
} 


const char* StateMachine::getState()
{
    switch (state) {
        case State::BOOT: return "BOOT";
        case State::OBTAIN: return "OBTAIN";
        case State::CAPDISPLAY: return "CAPABILITY";
        case State::NORMAL_PPS: return "NORMAL_PPS";
        case State::NORMAL_PDO: return "NORMAL_PDO";
        case State::NORMAL_QC: return "NORMAL_QC";
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

    voltageIncrementIndex = 0; // 20mV
    currentIncrementIndex = 0; // 50mA

    //Setup timmer:
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM0) ;
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ0, timerISR0) ;
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ0, true) ;
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + DELAY0 ;

    //Set up 1s timer using timer1
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM1) ;
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ1, timerISR1) ;
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ1, true) ;
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM1] = timer_hw->timerawl + DELAY1 ;
}

//Only need to declare static in header files
void StateMachine::encoderISR()
{
    encoder.tick();
}

// ISR for 100ms timer
void StateMachine::timerISR0()
{
    //Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM0);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + DELAY0 ;

    timerFlag0 = true;
}

// ISR for 1s timer
void StateMachine::timerISR1()
{
    //Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM1);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM1] = timer_hw->timerawl + DELAY1 ;

    timerFlag1 = !timerFlag1; //toggle
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
            menu.numPDO = usbpd.getNumPDO(); // Call after usbpd.begin()
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

void StateMachine::handleNormalPPSState()
{
    //Run once
    if(!normalPPSInitialized)
    {
        targetVoltage = 5000; //Default start up voltage
        targetCurrent = 1000; //Default start up current
        supply_adjust_mode = VOTLAGE_ADJUST;
        
        normalPPSInitialized = true;
        Serial.println( "Initialized NORMAL_PPS state" );
    }

    //START Routine 
    float ina_current_ma = abs(ina226.getCurrent_mA());
    
    if(timerFlag0)
    {
        if(digitalRead(pin_output_Enable))
        {
            updateOLED( ina226.getBusVoltage_mV() /1000.0, ina_current_ma/1000, true);
        }
        else
        {
            updateOLED(usbpd.readVoltage() /1000.0, ina_current_ma/1000, true);
        }
        timerFlag0 = false;
    }

    if(button_encoder.isButtonPressed() == 1)
    {
        if(supply_adjust_mode == VOTLAGE_ADJUST)
        {
            //Make sure the valve loop around
            voltageIncrementIndex = (voltageIncrementIndex + 1)% (sizeof(voltageIncrement)/sizeof(int));
        }
        else
        {
            currentIncrementIndex = (currentIncrementIndex + 1)% (sizeof(currentIncrement)/sizeof(int));
        }
    }
    if(button_selectVI.isButtonPressed() == 1)
    {
        if(supply_adjust_mode == VOTLAGE_ADJUST)    supply_adjust_mode = CURRENT_ADJUST;
        else                                        supply_adjust_mode = VOTLAGE_ADJUST;
    }
    
    if(button_output.isButtonPressed() == 1)
    {
        digitalWrite(pin_output_Enable, !digitalRead(pin_output_Enable));
    }
    process_request_voltage_current();
    update_supply_mode();
    //END Routine 
    Serial.println( "Handling NORMAL_PPS state" );
}

/**
 * In NormalPDO we only want to display
 * Reading Voltage and Current. We should also display max current allow
 */
void StateMachine::handleNormalPDOState()
{
    //Run once
    if(!normalPDOInitialized)
    {
        usbpd.setPDO(menu.menuPosition);
        //Now targetVoltage/targetCurrent is just for display. Do not pass over to AP33772
        targetVoltage = usbpd.getPDOVoltage(menu.menuPosition); //For display
        targetCurrent = usbpd.getPDOMaxcurrent(menu.menuPosition); //For display
        supply_adjust_mode = VOTLAGE_ADJUST;
        
        normalPDOInitialized = true;
        Serial.println( "Initialized NORMAL_PDO state" );
    }

    //Routine
    float ina_current_ma = abs(ina226.getCurrent_mA());
    
    if(timerFlag0)
    {
        if(digitalRead(pin_output_Enable))
        {
            updateOLED( ina226.getBusVoltage_mV() /1000.0, ina_current_ma/1000, true);
        }
        else
        {
            updateOLED(usbpd.readVoltage() /1000.0, ina_current_ma/1000, true);
        }
        timerFlag0 = false;
    }

    if(button_output.isButtonPressed() == 1)
    {
        digitalWrite(pin_output_Enable, !digitalRead(pin_output_Enable));
    }
    
    //END Routine 
    //Serial.println( "Handling NORMAL_PDO state" );
}

void StateMachine::handleNormalQCState()
{

}


void StateMachine::handleMenuState(){
    // Add MENU state routines here
    menu.page_selectCapability();
    //Check out https://github.com/shuzonudas/monoview/blob/master/U8g2/Examples/Menu/simpleMenu/simpleMenu.ino
    Serial.println( "Handling MENU state" );
}

void StateMachine::transitionTo(State newState)
{     
        //Ex: Print: Transitioning from BOOT to CAPABILITY
        Serial.print("Transitioning from ");
        Serial.print(getState());
        Serial.print("to ");
        Serial.println((newState == State::BOOT ? "BOOT" :newState == State::CAPDISPLAY ? "CAPABILITY" :newState == State::NORMAL_PPS ? "NORMAL" :"MENU"));

        state = newState;
        startTime = millis();

        if (newState == State::BOOT) {
            bootInitialized = false; // Reset the initialization flag when entering BOOT
        }
        if (newState == State::OBTAIN) {
            obtainInitialized = false; // Reset the initialization flag when entering OBTAIN
        }
        if (newState == State::CAPDISPLAY) {
            displayCapInitialized = false; // Reset the initialization flag when entering CAPDISPLAY
        }
        if (newState == State::NORMAL_PPS ||
            newState == State::NORMAL_PDO ||
            newState == State::NORMAL_QC  ||
            newState == State::MENU) {
            normalPPSInitialized = false; // Reset the initialization flag when entering CAPDISPLAY
            normalPDOInitialized = false;
            normalQCInitialized = false;
        }
        buttonPressed = false; // Reset button press flag when transitioning to another state
}


void StateMachine::printBootingScreen()
{
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 128/8, 64, image_PocketPD_Logo);
    u8g2.sendBuffer();
}

void StateMachine::updateOLED(float voltage, float current, uint8_t requestEN)
{
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    //Start-Fixed Component
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.drawStr(1, 14, "V");
    u8g2.drawStr(1, 47, "A");
    u8g2.setFont(u8g2_font_profont12_tr);
    switch (state){
        case State::NORMAL_PPS:
            u8g2.drawStr(110, 58, "PPS"); //TODO update according to mode
            break;
        case State::NORMAL_PDO:
            u8g2.drawStr(110, 58, "PDO"); //TODO update according to mode
            break;
        case State::NORMAL_QC:
            u8g2.drawStr(110, 58, "QC3.0"); //TODO update according to mode
            break;
    }
    //End-Fixed Component

    //Start-Dynamic component
    u8g2.setFont(u8g2_font_profont22_tr);
    sprintf(buffer, "%.2f", voltage);
    u8g2.drawStr(75-u8g2.getStrWidth(buffer), 14, buffer); //Right adjust
    sprintf(buffer, "%.2f", current);
    u8g2.drawStr(75-u8g2.getStrWidth(buffer), 47, buffer); //Right adjust

    //https://github.com/olikraus/u8g2/discussions/2028
    if(requestEN == 1){
    u8g2.setFont(u8g2_font_profont15_tr);
    sprintf(buffer, "%d mV", targetVoltage);
    u8g2.drawStr(75-u8g2.getStrWidth(buffer), 27, buffer); //Right adjust
    sprintf(buffer, "%d mA", targetCurrent);
    u8g2.drawStr(75-u8g2.getStrWidth(buffer), 62, buffer); //Right adjust
    }
    
    if(supply_mode) //CV = 0, CC = 1
    {
        u8g2.setFont(u8g2_font_profont12_tr);
        u8g2.drawStr(110, 10, "CV");
        u8g2.drawStr(110, 23, "CC");
        u8g2.setDrawColor(2);
        u8g2.drawBox(104, 13, 23, 12); // CC
    }
    else
    {
        u8g2.setFont(u8g2_font_profont12_tr);
        u8g2.drawStr(110, 10, "CV");
        u8g2.drawStr(110, 23, "CC");
        u8g2.setDrawColor(2);
        u8g2.drawBox(104, 0, 23, 12); // CV
    }
    //End-Dynamic component

    //Blinking cursor
    if(timerFlag1 && (state==State::NORMAL_PPS || state == State::NORMAL_QC))
    {
        if(supply_adjust_mode == VOTLAGE_ADJUST)
            u8g2.drawLine(voltage_cursor_position[voltageIncrementIndex],28,voltage_cursor_position[voltageIncrementIndex] + 5,28);
        else
            u8g2.drawLine(current_cursor_position[currentIncrementIndex],63,current_cursor_position[currentIncrementIndex] + 5,63);
    }

    u8g2.sendBuffer();
}

void StateMachine::update_supply_mode()
{
    
    if( state == State::OBTAIN && 
        usbpd.existPPS && 
        (targetVoltage >= (vbus_voltage + ina_current_ma*0.314 + 50)) && 
        digitalRead(pin_output_Enable) ) //0.25Ohm for max round trip resistance + 0.02 Ohm for connector + 0.044 Ohm switch + 50mV margin
    supply_mode = MODE_CC;
    else // Other state only display CV mode
    supply_mode = MODE_CV; 
}


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

/**
 * update_request_voltage_current
 * @brief Read changes from encoder, compute request voltage/current
 * Sent request to AP33772 to update
 */
void StateMachine::process_request_voltage_current()
{
  static int val;
  if(val = (int8_t)encoder.getDirection())
  {
    if(supply_adjust_mode == VOTLAGE_ADJUST)
    {
      targetVoltage = targetVoltage + val*voltageIncrement[voltageIncrementIndex];
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
      targetCurrent = targetCurrent + val*currentIncrement[currentIncrementIndex];
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