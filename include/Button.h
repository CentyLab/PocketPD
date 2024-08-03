/**
 * Implement long press detection using ezButton library
 */

#include <ezButton.h>

#ifndef BUTTON_H
#define BUTTON_H

class Button
{
public:
    int isButtonPressed(void);
    Button(int buttonNum, int mode) : button(buttonNum, mode) { button.setDebounceTime(debounceTime); }
    Button(int buttonNum) : button(buttonNum, INPUT_PULLUP) { button.setDebounceTime(debounceTime); }
    void clearLongPressedFlag(void) { longPressedFlag = false; } // Call after a succesful long press trigger
    void setDebounceTime(unsigned long time);
    bool longPressedFlag = false;
    void loop();

private:
    ezButton button;
    unsigned long pressedTime = 0;
    unsigned long releasedTime = 0;
    bool isPressing = false;
    bool isLongDetected = false;
    const int debounceTime = 50;                 // Set default debounce time to 50ms
    const unsigned long SHORT_PRESS_TIME = 1000; // Short is less than 1s, more than 50ms
    const unsigned long LONG_PRESS_TIME = 3000;  // Long is longer than 4s
};

#endif