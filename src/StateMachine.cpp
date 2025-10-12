#include <StateMachine.h>
#include <math.h>

// Static object require separate implementation
RotaryEncoder StateMachine::encoder(pin_encoder_A, pin_encoder_B, RotaryEncoder::LatchMode::FOUR3);
bool StateMachine::timerFlag0 = false; // Need to initilize Static variable
bool StateMachine::timerFlag1 = false; // Need to initilize Static variable

/**
 * @brief Transistion conditions between states. Call in main.cpp
 */
void StateMachine::update()
{
    auto now = millis();
    auto elapsed = now - startTime;

    switch (state)
    {
    case State::BOOT:
        handleBootState();
        if (elapsed >= BOOT_TO_OBTAIN_TIMEOUT)
            transitionTo(State::OBTAIN);
        
        break;

    case State::OBTAIN:
        handleObtainState();
        if(encoder.getDirection() != RotaryEncoder::Direction::NOROTATION) { // Encoder rotation
            transitionTo(State::MENU);
        } else if (button_encoder.isButtonPressed() | button_output.isButtonPressed() | button_selectVI.isButtonPressed()) // Short press
            handleInitialMode();
        else if (elapsed >= OBTAIN_TO_CAPDISPLAY_TIMEOUT)
            transitionTo(State::CAPDISPLAY);
        
        break;

    case State::CAPDISPLAY:
        handleDisplayCapState();
        if(encoder.getDirection() != RotaryEncoder::Direction::NOROTATION) { // Encoder rotation
            transitionTo(State::MENU);
        } else if ((button_encoder.isButtonPressed() | button_output.isButtonPressed() | button_selectVI.isButtonPressed() | elapsed >= DISPLAYCCAP_TO_NORMAL_TIMEOUT)) // Short press + timeout
            handleInitialMode();
        
        break;

    case State::NORMAL_PPS:
        handleNormalPPSState();
        if (button_selectVI.longPressedFlag) // Long press isButtonPressed() is called inside the state
        {
            button_selectVI.clearLongPressedFlag();
            transitionTo(State::MENU);
        }
        saveSettingsToEEPROM();
        break;
    case State::NORMAL_PDO:
        handleNormalPDOState();
        button_selectVI.isButtonPressed();   // Call to update long press flag
        if (button_selectVI.longPressedFlag) // Long press isButtonPressed() is called inside the state
        {
            button_selectVI.clearLongPressedFlag();
            transitionTo(State::MENU);
        }
        saveSettingsToEEPROM();
        break;
    case State::NORMAL_QC:
        handleNormalQCState();
        if (button_selectVI.longPressedFlag) // Long press isButtonPressed() is called inside the state
        {
            button_selectVI.clearLongPressedFlag();
            transitionTo(State::MENU);
        }
        saveSettingsToEEPROM();
        break;

    case State::MENU:
        if(usbpd.getNumPDO() == 0) {
            transitionTo(State::NORMAL_PDO);
            break; // No PDO available, go back to NORMAL_PDO state
        }
        handleMenuState();

        button_encoder.isButtonPressed();
        button_output.isButtonPressed();
        button_selectVI.isButtonPressed();

        if (button_selectVI.longPressedFlag) // Long press
        {
            button_selectVI.clearLongPressedFlag();
            transitionTo(State::NORMAL_PPS);
        }
        if (button_encoder.longPressedFlag) // Long press
        {
            button_encoder.clearLongPressedFlag();
            if (menu.menuPosition == usbpd.getPPSIndex()) {
                transitionTo(State::NORMAL_PPS);
            } else {
                forceSave = true;
                transitionTo(State::NORMAL_PDO);
            }   
        }
        break;
    }
}

/**
 * @return Return current state in string
 */
const char *StateMachine::getState()
{
    switch (state)
    {
    case State::BOOT:
        return "BOOT";
    case State::OBTAIN:
        return "OBTAIN";
    case State::CAPDISPLAY:
        return "CAPABILITY";
    case State::NORMAL_PPS:
        return "NORMAL_PPS";
    case State::NORMAL_PDO:
        return "NORMAL_PDO";
    case State::NORMAL_QC:
        return "NORMAL_QC";
    case State::MENU:
        return "MENU";
    default:
        return "UNKNOWN";
    }
}

void StateMachine::componentInit()
{
    Wire.setSDA(pin_SDA);
    Wire.setSCL(pin_SCL);

    attachInterrupt(digitalPinToInterrupt(pin_encoder_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pin_encoder_B), encoderISR, CHANGE);

    pinMode(pin_button_outputSW, INPUT_PULLUP);
    pinMode(pin_button_selectVI, INPUT_PULLUP);

    pinMode(pin_output_Enable, OUTPUT); // Load Switch
    digitalWrite(pin_output_Enable, LOW);
    output_display_mode = OUTPUT_NORMAL;

    button_encoder.setDebounceTime(50); // set debounce time to 50 milliseconds
    button_output.setDebounceTime(50);
    button_selectVI.setDebounceTime(50);

    u8g2.begin();
    ina226.begin();
    // ina226.setMaxCurrentShunt(6, SENSERESISTOR);
    // configure(shunt, current_LSB_mA, current_zero_offset_mA, bus_V_scaling_e4)
    #ifdef HW1_0
    ina226.configure(0.01023, 0.25, 6.4, 9972); //Factory calibration
    #endif
    
    #ifdef HW1_1
    ina226.configure(0.00528, 0.25, 10.4, 9972); //Factory calibration
    #endif

    voltageIncrementIndex = 2; // 0= 20mV, 1 = 100mV, 2 = 1000mV
    currentIncrementIndex = 2; // 0= 50mA, 1 = 100mA, 2 = 1000mA

    // Set up 100ms timer using timer0
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM0);
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ0, timerISR0);
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ0, true);
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + DELAY0;

    // Set up 1s timer using timer1
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM1);
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ1, timerISR1);
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ1, true);
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM1] = timer_hw->timerawl + DELAY1;

    // Init EEPROM
    EEPROM.begin(EEPROM_SIZE);
}

// Only need to declare static in header files
void StateMachine::encoderISR()
{
    encoder.tick();
}

// ISR for 33ms timer
void StateMachine::timerISR0()
{
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM0);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + DELAY0;

    timerFlag0 = true;
}

// ISR for 500ms timer
void StateMachine::timerISR1()
{
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM1);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM1] = timer_hw->timerawl + DELAY1;

    timerFlag1 = !timerFlag1; // toggle
}

/**
 * @brief Tasks for Boot state
 */
void StateMachine::handleBootState()
{
    if (!bootInitialized)
    {
        //* BEGIN Only run once when entering the state */
        // led.turnOn(); // Turn on the LED when entering the BOOT state
        componentInit();
        printBootingScreen();
        // ADD Check if connected to PC here
        //* END Only run once when entering the state */
        bootInitialized = true; // Mark BOOT state as initialized
        Serial.println("Initialized BOOT state");
    }
    //* BEGIN state routine */
    // Add additional BOOT state routines here

    //* END state routine */
    //Serial.println("Handling BOOT state");
}

/**
 * @brief Tasks for Obtain state
 */
void StateMachine::handleObtainState()
{
    if (!obtainInitialized)
    {
        //* BEGIN Only run once when entering the state */
        usbpd.begin();                   // Start pulling the PDOs from power supply
        menu.numPDO = usbpd.getNumPDO(); // Call after usbpd.begin()
        // ADD check QC3.0 code here
        //* END Only run once when entering the state */
        bootInitialized = true; // Mark BOOT state as initialized
        Serial.println("Initialized BOOT state");
    }
    //* BEGIN state routine */
    // Add additional BOOT state routines here

    //* END state routine */
    // Serial.println( "Handling BOOT state" );
}

/**
 * @brief Tasks for DisplayCapability state
 */
void StateMachine::handleDisplayCapState()
{
    if (!displayCapInitialized)
    {
        //* BEGIN Only run once when entering the state */
        // led.turnOn(); // Turn on the LED when entering the BOOT state
        printProfile();
        usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);

        //* END Only run once when entering the state */
        displayCapInitialized = true;
    }
    // Add CAPABILITY state routines here
    // Serial.println( "Handling CAPABILITY state" );
}

/**
 * @brief Tasks for Normal with PPS state
 */
void StateMachine::handleNormalPPSState()
{
    // Run once
    if (!normalPPSInitialized)
    {
        //* BEGIN Only run once when entering the state */
        

        // Load settings from EEPROM
        if(loadSettingsFromEEPROM(true)) {
            usbpd.checkVoltageCurrent(targetVoltage, targetCurrent);
        } else {
            targetVoltage = 5000; // Default start up voltage
            targetCurrent = 1000; // Default start up current
        }
        supply_adjust_mode = VOTLAGE_ADJUST;
        usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);

        //* END Only run once when entering the state */
        normalPPSInitialized = true;
        Serial.println("Initialized NORMAL_PPS state");
    }

    //* BEGIN state routine */
    if (timerFlag0) // Timer with DELAY0
    {
        ina_current_ma = abs(ina226.getCurrent_mA());
        vbus_voltage_mv = ina226.getBusVoltage_mV();

        //Place before updateOLED to prevent voltage/current go out of bound, then recover.

        process_request_voltage_current();

        float currentReading = 0;
        float voltageReading = 0;
        if (digitalRead(pin_output_Enable)){
            currentReading = ina_current_ma / 1000.0;
            voltageReading = vbus_voltage_mv / 1000.0;
        } else {
            currentReading = 0;
            voltageReading = usbpd.readVoltage() / 1000.0;
        }

        // Update energy accumulation
        updateEnergyAccumulation(voltageReading, currentReading);

        if (output_display_mode == OUTPUT_ENERGY)
        {
            // Show energy tracking display
            updateOLED_Energy(voltageReading, currentReading);
        }
        else if (output_display_mode == OUTPUT_NORMAL)
        {
            // Show normal display with output enabled
            updateOLED(voltageReading, currentReading,true);
        }


        update_supply_mode();
        timerFlag0 = false;
    }

    // Disable encoder button and selectVI button in energy display mode
    if (output_display_mode != OUTPUT_ENERGY)
    {
        if (button_encoder.isButtonPressed() == 1)
        {
            if (supply_adjust_mode == VOTLAGE_ADJUST)
            {
                // Make sure the valve loop around
                voltageIncrementIndex = (voltageIncrementIndex + 1) % (sizeof(voltageIncrement) / sizeof(int));
            }
            else
            {
                currentIncrementIndex = (currentIncrementIndex + 1) % (sizeof(currentIncrement) / sizeof(int));
            }
        }
        if (button_selectVI.isButtonPressed() == 1)
        {
            if (supply_adjust_mode == VOTLAGE_ADJUST)
                supply_adjust_mode = CURRENT_ADJUST;
            else
                supply_adjust_mode = VOTLAGE_ADJUST;
        }

        process_encoder_input();
    }

    process_output_button();

    //* END state routine */
    // Serial.println( "Handling NORMAL_PPS state" );
}

/**
 * @brief Tasks for Normal with fixed PDO state
 */
void StateMachine::handleNormalPDOState()
{
    // Run once
    if (!normalPDOInitialized)
    {
        //* BEGIN Only run once when entering the state */
        usbpd.setPDO(menu.menuPosition);
        // Now targetVoltage/targetCurrent is just for display. Do not pass over to AP33772
        targetVoltage = usbpd.getPDOVoltage(menu.menuPosition);    // For display
        targetCurrent = usbpd.getPDOMaxcurrent(menu.menuPosition); // For display
        supply_adjust_mode = VOTLAGE_ADJUST;

        //* END Only run once when entering the state */
        normalPDOInitialized = true;
        Serial.println(usbpd.getNumPDO());
        Serial.println("Initialized NORMAL_PDO state");
    }

    //* BEGIN state routine */
    float ina_current_ma = abs(ina226.getCurrent_mA());

    if (timerFlag0)
    {
        ina_current_ma = abs(ina226.getCurrent_mA());
        vbus_voltage_mv = ina226.getBusVoltage_mV();

        float currentReading = 0;
        float voltageReading = 0;
        if (digitalRead(pin_output_Enable)){
            currentReading = ina_current_ma / 1000.0;
            voltageReading = vbus_voltage_mv / 1000.0;
        } else {
            currentReading = 0;
            voltageReading = usbpd.readVoltage() / 1000.0;
        }
        // Update energy accumulation
        updateEnergyAccumulation(voltageReading, currentReading);

        if (output_display_mode == OUTPUT_ENERGY)
        {
            // Show energy tracking display
            updateOLED_Energy(voltageReading, currentReading);
        }
        else if (output_display_mode == OUTPUT_NORMAL)
        {
            // Show normal display with output enabled
            updateOLED(voltageReading, currentReading,true);
        }
        update_supply_mode();
        timerFlag0 = false;
    }

    process_output_button();

    //* END state routine */
    // Serial.println( "Handling NORMAL_PDO state" );
}

/**
 * @brief tasks for Normal with QC3.0 state
 */
void StateMachine::handleNormalQCState()
{
    // Run once
    if (!normalQCInitialized)
    {
        //* BEGIN Only run once when entering the state */
        // TODO QC implmentation
        //* END Only run once when entering the state */
        normalQCInitialized = true;
        Serial.println("Initialized NORMAL_QC state");
    }

    //* BEGIN state routine */
    float ina_current_ma = abs(ina226.getCurrent_mA());

    if (timerFlag0)
    {

        vbus_voltage_mv = ina226.getBusVoltage_mV();

        // Update energy accumulation
        float currentReading = 0;
        float voltageReading = 0;
        if (digitalRead(pin_output_Enable)){
            currentReading = ina_current_ma / 1000.0;
            voltageReading = vbus_voltage_mv / 1000.0;
        } else {
            currentReading = 0;
            voltageReading = usbpd.readVoltage() / 1000.0;
        }
        // Update energy accumulation
        updateEnergyAccumulation(voltageReading, currentReading);

        if (output_display_mode == OUTPUT_ENERGY)
        {
            // Show energy tracking display
            updateOLED_Energy(voltageReading, currentReading);
        }
        else if (output_display_mode == OUTPUT_NORMAL)
        {
            // Show normal display with output enabled
            updateOLED(voltageReading, currentReading,true);
        }        
        timerFlag0 = false;
    }

    process_output_button();
    // TODO QC implmentation

    //* END state routine */
    // Serial.println( "Handling NORMAL_QC state" );
}

/**
 * @brief Tasks for Menu state. Just a wrapper function
 */
void StateMachine::handleMenuState()
{
    //* BEGIN state routine */
    menu.page_selectCapability();
    //* END state routine */
    // Check out https://github.com/shuzonudas/monoview/blob/master/U8g2/Examples/Menu/simpleMenu/simpleMenu.ino
    // Serial.println( "Handling MENU state" );
}

/**
 * @brief transitionTo perform flags clear before transisiton. Enable entry tasks in each state.
 * @param State::newstate input the next state to transistion
 */
void StateMachine::transitionTo(State newState)
{
    // Ex: Print: Transitioning from BOOT to CAPABILITY
    Serial.print("Transitioning from ");
    Serial.print(getState());
    Serial.print("to ");
    Serial.println((newState == State::BOOT ? "BOOT" : newState == State::CAPDISPLAY ? "CAPABILITY"
                                                   : newState == State::NORMAL_PPS   ? "NORMAL"
                                                                                     : "MENU"));

    state = newState;
    startTime = millis();

    if (newState == State::BOOT)
    {
        bootInitialized = false; // Reset the initialization flag when entering BOOT
    }
    if (newState == State::OBTAIN)
    {
        obtainInitialized = false; // Reset the initialization flag when entering OBTAIN
    }
    if (newState == State::CAPDISPLAY)
    {
        displayCapInitialized = false; // Reset the initialization flag when entering CAPDISPLAY
    }
    if (newState == State::NORMAL_PPS ||
        newState == State::NORMAL_PDO ||
        newState == State::NORMAL_QC ||
        newState == State::MENU)
    {
        normalPPSInitialized = false; // Reset the initialization flag when entering CAPDISPLAY
        normalPDOInitialized = false;
        normalQCInitialized = false;
    }
    buttonPressed = false; // Reset button press flag when transitioning to another state
}

void StateMachine::printBootingScreen()
{
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 128 / 8, 64, image_PocketPD_Logo);
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.drawStr(67,64, "FW: ");
    u8g2.drawStr(87,64, VERSION);
    u8g2.sendBuffer();
}

/**
 * @brief Update value on OLED screen
 * @param voltage voltage (mV) to display, big text
 * @param current current (mA) to display, big text
 * @param requestEN flag to show/not show smaller text. Used for PPS request.
 */
void StateMachine::updateOLED(float voltage, float current, uint8_t requestEN)
{
    // TODO Can optimize away requestEN
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // Start-Fixed Component
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.drawStr(1, 14, "V");
    u8g2.drawStr(1, 47, "A");
    u8g2.setFont(u8g2_font_profont12_tr);
    switch (state)
    {
    case State::NORMAL_PPS:
        u8g2.drawStr(110, 62, "PPS");
        break;
    case State::NORMAL_PDO:
        u8g2.drawStr(110, 62, "PDO");
        break;
    case State::NORMAL_QC:
        u8g2.drawStr(110, 62, "QC3.0");
        break;
    }
    // End-Fixed Component

    // Start-Dynamic component
    u8g2.setFont(u8g2_font_profont22_tr);
    sprintf(buffer, "%.2f", voltage);
    u8g2.drawStr(75 - u8g2.getStrWidth(buffer), 14, buffer); // Right adjust
    sprintf(buffer, "%.2f", current);
    u8g2.drawStr(75 - u8g2.getStrWidth(buffer), 47, buffer); // Right adjust

    // https://github.com/olikraus/u8g2/discussions/2028
    if (requestEN == 1)
    {
        u8g2.setFont(u8g2_font_profont15_tr);
        sprintf(buffer, "%d mV", targetVoltage);
        u8g2.drawStr(75 - u8g2.getStrWidth(buffer), 27, buffer); // Right adjust
        sprintf(buffer, "%d mA", targetCurrent);
        u8g2.drawStr(75 - u8g2.getStrWidth(buffer), 62, buffer); // Right adjust
    }

    if (supply_mode) // CV = 0, CC = 1
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
    // End-Dynamic component

    // Blinking cursor
    if (timerFlag1 && (state == State::NORMAL_PPS || state == State::NORMAL_QC))
    {
        if (supply_adjust_mode == VOTLAGE_ADJUST)
            u8g2.drawLine(voltage_cursor_position[voltageIncrementIndex], 28, voltage_cursor_position[voltageIncrementIndex] + 5, 28);
        else
            u8g2.drawLine(current_cursor_position[currentIncrementIndex], 63, current_cursor_position[currentIncrementIndex] + 5, 63);
    }

    // Blinking output arrow
    if(digitalRead(pin_output_Enable) == 1)
    {
        u8g2.drawXBMP(105, 28, 20, 20, arrow_bitmapallArray[counter_gif]); // draw arrow animation
        counter_gif = (counter_gif + 1) % 28;
    }

    u8g2.sendBuffer();
}

/**
 * @brief Update energy accumulation (Wh and Ah) - continuous tracking, only resets on power cycle
 */
void StateMachine::updateEnergyAccumulation(float voltage, float current)
{
    if (digitalRead(pin_output_Enable) == 1)
    {
        unsigned long currentTime = millis();

        // Start new session if needed
        if (currentSessionStartTime == 0)
        {
            currentSessionStartTime = currentTime;
            lastEnergyUpdate = currentTime;
            return;
        }

        // Accumulate energy since last update - ALWAYS accumulate when output enabled
        unsigned long deltaMs = currentTime - lastEnergyUpdate;

        // Only accumulate if we have a reasonable delta (not first call edge case, not huge jump)
        if (deltaMs > 0 && deltaMs < 1000)
        { // Between 0ms and 1 second
            if (!isfinite(voltage) || !isfinite(current))
            {
                lastEnergyUpdate = currentTime;
                return;
            }

            double voltage_v = static_cast<double>(voltage);
            double current_a = static_cast<double>(current);

            if (voltage_v >= 0.0 && current_a >= 0.0)
            {
                double power_w = voltage_v * current_a;
                double deltaHours = static_cast<double>(deltaMs) / 3600000.0;

                accumulatedWh += power_w * deltaHours;
                accumulatedAh += current_a * deltaHours;
            }
        }

        lastEnergyUpdate = currentTime;
    }
    else
    {
        // Output disabled - save current session time to historical
        if (currentSessionStartTime > 0)
        {
            unsigned long sessionDuration = (millis() - currentSessionStartTime) / 1000;
            historicalOutputTime += sessionDuration;
            currentSessionStartTime = 0;
        }
        lastEnergyUpdate = 0;
    }
}

/**
 * @brief Energy tracking display - shows Wh/Ah with compact V/A at top
 */
void StateMachine::updateOLED_Energy(float voltage, float current)
{
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // Top section - Compact real-time V/A
    u8g2.setFont(u8g2_font_profont12_tr);
    sprintf(buffer, "%.2fV", voltage);
    u8g2.drawStr(2, 10, buffer);
    sprintf(buffer, "%.2fA", current);
    u8g2.drawStr(48, 10, buffer);

    // CV/CC indicator (same size and alignment as V/A)
    if (supply_mode == MODE_CC)
    {
        u8g2.drawStr(94, 10, "CC");
    }
    else
    {
        u8g2.drawStr(94, 10, "CV");
    }

    // Animated arrow (only when output is enabled)
    if (digitalRead(pin_output_Enable) == 1)
    {
        u8g2.drawXBMP(108, 0, 20, 20, arrow_bitmapallArray[counter_gif]);
        counter_gif = (counter_gif + 1) % 28;
    }

    // Calculate total time: historical + current session
    unsigned long totalSeconds = historicalOutputTime;
    if (currentSessionStartTime > 0)
    {
        totalSeconds += (millis() - currentSessionStartTime) / 1000;
    }

    // Format time based on totalSeconds (only increments when outputting)
    char timeBuffer[10];
    if (totalSeconds < 60)
    {
        // Under 1 minute: "00:SS"
        sprintf(timeBuffer, "00:%02lu", totalSeconds);
    }
    else if (totalSeconds < 3600)
    {
        // Under 1 hour: "MM:SS"
        sprintf(timeBuffer, "%02lu:%02lu", totalSeconds / 60, totalSeconds % 60);
    }
    else if (totalSeconds < 86400)
    {
        // Under 1 day: "#h##m" (e.g., "2h45m")
        unsigned long hours = totalSeconds / 3600;
        unsigned long minutes = (totalSeconds % 3600) / 60;
        sprintf(timeBuffer, "%luh%02lum", hours, minutes);
    }
    else
    {
        // 1+ days: "#d##h" (e.g., "3d12h")
        unsigned long days = totalSeconds / 86400;
        unsigned long hours = (totalSeconds % 86400) / 3600;
        sprintf(timeBuffer, "%lud%02luh", days, hours);
    }

    // Use larger font for main data rows
    u8g2.setFont(u8g2_font_profont17_tr);

    // Left column: Watts (no decimal when >= 100)
    float watts = voltage * current;
    if (watts >= 100.0)
    {
        sprintf(buffer, "%.0f", watts);
    }
    else
    {
        sprintf(buffer, "%.1f", watts);
    }
    u8g2.drawStr(2, 35, buffer);
    int watts_width = u8g2.getStrWidth(buffer);
    u8g2.drawStr(2 + watts_width + 2, 35, "W"); // 2px spacing

    // Time (more spacing from watts)
    u8g2.drawStr(2, 55, timeBuffer);

    const double totalWh = accumulatedWh;
    const double totalAh = accumulatedAh;

    // Right column: Wh with proper significant digits
    if (totalWh < 10.0)
    {
        sprintf(buffer, "%.2f", totalWh);
    }
    else if (totalWh < 100.0)
    {
        sprintf(buffer, "%.1f", totalWh);
    }
    else
    {
        sprintf(buffer, "%.0f", totalWh);
    }
    u8g2.drawStr(70, 35, buffer);
    int wh_width = u8g2.getStrWidth(buffer);
    u8g2.drawStr(70 + wh_width + 2, 35, "Wh"); // 2px spacing

    // Ah with proper significant digits (more spacing)
    if (totalAh < 10.0)
    {
        sprintf(buffer, "%.2f", totalAh);
    }
    else if (totalAh < 100.0)
    {
        sprintf(buffer, "%.1f", totalAh);
    }
    else
    {
        sprintf(buffer, "%.0f", totalAh);
    }
    u8g2.drawStr(70, 55, buffer);
    int ah_width = u8g2.getStrWidth(buffer);
    u8g2.drawStr(70 + ah_width + 2, 55, "Ah"); // 2px spacing

    // Reset font to default
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.sendBuffer();
}

/** Need fixing */
void StateMachine::update_supply_mode()
{
    //Serial.println("Dumping start ");
    //Serial.println(targetVoltage);
    //Serial.println(vbus_voltage_mv);
    //Serial.println(ina_current_ma);
    //Serial.println(targetCurrent);
    if(state == State::NORMAL_PPS && digitalRead(pin_output_Enable)                         &&
        (targetVoltage >= vbus_voltage_mv + 50 + ina_current_ma*(0.166 + 0.022 + 0.005))    && 
        (ina_current_ma >= targetCurrent - 150 && ina_current_ma <= targetCurrent + 150))   //Current read when hitting limit, need to be around 85-115% limit set
        supply_mode = MODE_CC;
    else // Other state only display CV mode
        supply_mode = MODE_CV;
}

/**
 * @brief Print out PPS/PDO profiles onto OLED
 */
void StateMachine::printProfile()
{
    int linelocation = 9;
    /**
     * Print out PPS if exist, else, print out PDOs
     * Missing: cannot display multiple PPS profile
     */
    u8g2.clearBuffer();

    if (usbpd.getNumPDO() == 0)
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
    // Print PDOs onto OLED
    for (byte i = 0; i < usbpd.getNumPDO(); i++)
    {
        if (i != usbpd.getPPSIndex())
        {
            linelocation = 9 * (i + 1);
            u8g2.setCursor(5, linelocation);
            u8g2.print("PDO: ");
            u8g2.print(usbpd.getPDOVoltage(i) / 1000.0, 0);
            u8g2.print("V @ ");
            u8g2.print(usbpd.getPDOMaxcurrent(i) / 1000.0, 0);
            u8g2.print("A");
        }
        else if (usbpd.existPPS)
        {
            linelocation = 9 * (i + 1);
            u8g2.setCursor(5, linelocation);
            u8g2.print("PPS: ");
            u8g2.print(usbpd.getPPSMinVoltage(i) / 1000.0, 1);
            u8g2.print("V~");
            u8g2.print(usbpd.getPPSMaxVoltage(i) / 1000.0, 1);
            u8g2.print("V @ ");
            u8g2.print(usbpd.getPPSMaxCurrent(i) / 1000.0, 0);
            u8g2.print("A");
        }
    }
    u8g2.sendBuffer();
}

/**
 * @brief Read changes from encoder, compute target voltage/current
 * Just update targetVoltage and targetCurrent
 */
void StateMachine::process_encoder_input()
{
    static int val;
    if (val = (int8_t)encoder.getDirection())
    {
        if (supply_adjust_mode == VOTLAGE_ADJUST) //If cursor is at voltage
        {
            targetVoltage = targetVoltage + val * voltageIncrement[voltageIncrementIndex];
        }
        else                                      //If cursor is at current
        {
            targetCurrent = targetCurrent + val * currentIncrement[currentIncrementIndex];
        }
        updateSaveStamp(); // Update the save stamp to current time to debounce too many writes to EEPROM
    }
}

/**
 * @brief Handle output button press
 * Short press: Toggle output on/off (never changes display mode)
 * Long press: Toggle between NORMAL and ENERGY display modes (never changes output)
 */
void StateMachine::process_output_button()
{
    int buttonState = button_output.isButtonPressed();

    // Check for long press flag first
    if (button_output.longPressedFlag)
    {
        button_output.clearLongPressedFlag();
        // Toggle between NORMAL and ENERGY display modes (output state unchanged)
        
        if (output_display_mode == OUTPUT_NORMAL)
        {   // Ignore all encoder turning
            output_display_mode = OUTPUT_ENERGY;
            detachInterrupt(digitalPinToInterrupt(pin_encoder_A));
            detachInterrupt(digitalPinToInterrupt(pin_encoder_B));
        }
        else // In OUTPUT_ENERGY
        {   // Now read all encoder turning
            output_display_mode = OUTPUT_NORMAL;
            attachInterrupt(digitalPinToInterrupt(pin_encoder_A), encoderISR, CHANGE);
            attachInterrupt(digitalPinToInterrupt(pin_encoder_B), encoderISR, CHANGE);      
        }
    }
    else if (buttonState == 1) // Short press
    {
        // Toggle output on/off (display mode unchanged)
        if (digitalRead(pin_output_Enable) == LOW) // Currently off
        {
            digitalWrite(pin_output_Enable, HIGH);
        }
        else // Currently on
        {
            digitalWrite(pin_output_Enable, LOW);
        }
    }
}

/**
 * @brief Read changes from encoder, compute request voltage/current
 * Sent request to AP33772 to update
 */
void StateMachine::process_request_voltage_current()
{
    static int val;
        if (supply_adjust_mode == VOTLAGE_ADJUST)
        {
            if (usbpd.existPPS)
            {
                if ((float)usbpd.getPPSMinVoltage(usbpd.getPPSIndex()) <= targetVoltage && (float)usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()) >= targetVoltage)
                    // usbpd.setVoltage(targetVoltage);
                    usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
                else if (usbpd.getPPSMinVoltage(usbpd.getPPSIndex()) > targetVoltage)
                    targetVoltage = usbpd.getPPSMinVoltage(usbpd.getPPSIndex()); // No change
                else if (usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()) < targetVoltage)
                    targetVoltage = usbpd.getPPSMaxVoltage(usbpd.getPPSIndex()); // No change
            }
            else
            { // PDOs only has profile between 5V and 20V
                if (targetVoltage > 20000)
                    targetVoltage = 20000;
                else if (targetVoltage < 5000)
                    targetVoltage = 5000;
                usbpd.setVoltage(targetVoltage);
            }
        }
        else
        {
            if (usbpd.existPPS)
            {
                if (targetCurrent <= 1000)
                    targetCurrent = 1000; // Cap at 100mA minimum, no current update
                else if (usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()) >= targetCurrent)
                    // usbpd.setMaxCurrent(targetCurrent);
                    usbpd.setSupplyVoltageCurrent(targetVoltage, targetCurrent);
                else if (usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()) < targetCurrent)
                    targetCurrent = usbpd.getPPSMaxCurrent(usbpd.getPPSIndex()); // No change
            }
            else
                targetCurrent = usbpd.getMaxCurrent(); // Pull current base on current PDO
        }
}

/**
 * @brief Handle the initial mode when the device starts up
 * This function checks if settings can be loaded from EEPROM and transitions to the appropriate state.
 */
void StateMachine::handleInitialMode() {
    if(loadSettingsFromEEPROM(false)) {
        if(menu.menuPosition == usbpd.getPPSIndex())
            transitionTo(State::NORMAL_PPS);
        else
            transitionTo(State::NORMAL_PDO);
    } else {
        if(usbpd.existPPS)
            transitionTo(State::NORMAL_PPS);
        else
            transitionTo(State::NORMAL_PDO);
    }
}

/**
 * @brief Save settings to EEPROM
 * @return true if settings were saved successfully, false otherwise
 */
bool StateMachine::saveSettingsToEEPROM() {
    uint32_t currentTime = millis();
    bool ret = false;
    bool saveVC = state == State::NORMAL_PPS; // Check if we are in NORMAL_PPS state to save PPS settings

    if(forceSave || (currentTime - saveStamp >= EEPROM_SAVE_INTERVAL)) {
        saveStamp = currentTime;
        if(usbpd.getNumPDO() == 0) {
            Serial.println("No PDOs available, cannot save settings.");
            return false; // No PDOs available, cannot save settings
        }
        Settings newSettings = { 0 };
        eepromHandler.loadSettings(newSettings); // Load current settings from EEPROM
        if(saveVC) {
            newSettings.targetVoltage = targetVoltage;
            newSettings.targetCurrent = targetCurrent;
            int32_t menuIndex = menu.menuPosition;
            if(menuIndex == 0) { menuIndex = usbpd.getPPSIndex(); } // If menu position is 0, use PPS index
            newSettings.menuPosition  = menuIndex; // Save current menu position
        } else {
            newSettings.menuPosition = menu.menuPosition;    // Save current menu position
        }
        
        ret = eepromHandler.saveSettings(newSettings);
        Serial.printf("Settings saved to EEPROM: %d\n\r", ret);
        forceSave = !ret; // If save failed, set forceSave to true to retry next time
    }
    return ret;
}

/**
 * @brief Load settings from EEPROM
 * @param vc If true, load Voltage/Current settings, otherwise load menu position
 * @return true if settings were loaded successfully, false otherwise
 */
bool StateMachine::loadSettingsFromEEPROM(bool vc) {
    Settings loadedSettings;
    bool ret = eepromHandler.loadSettings(loadedSettings);
    if(ret) {
        if(!vc) {
            menu.menuPosition = loadedSettings.menuPosition; // Load menu position
            menu.checkMenuPosition(false);
        } else {
            targetVoltage      = loadedSettings.targetVoltage;
            targetCurrent      = loadedSettings.targetCurrent;
        }
        Serial.printf("Settings loaded from EEPROM: mV=%4d, mI=%4d, Menu position=%d\n\r", 
                      targetVoltage, targetCurrent, menu.menuPosition);
    } else {
        Serial.println("Failed to load settings from EEPROM.");
    }
    return ret;
}

/**
 * @brief Debounce the save stamp
 * 
 */
void StateMachine::updateSaveStamp() {
    saveStamp = millis(); // Update the save stamp to current time
}