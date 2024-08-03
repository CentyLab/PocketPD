#include <Button.h>


/**
 * Return   0 if button not pressed
 *          1 if button is short pressed
 *          2 if button is long pressed
*/
int Button::isButtonPressed (void)
{
    button.loop(); //Must call first

    if(button.isPressed())
    {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
    }

    if(button.isReleased())
    {
        isPressing = false;
        releasedTime = millis();
        
        long pressDuration = releasedTime - pressedTime;

        if(pressDuration < SHORT_PRESS_TIME)
            return 1;
    }

    //TODO: Trim implementation, right now you have two way of signaling longpress
    // One is the return of the function, one is the longPressedFlag
    if(isPressing == true && isLongDetected == false)
    {
        long pressDuration = millis() - pressedTime;

        if(pressDuration > LONG_PRESS_TIME)
        {
            isLongDetected = true;
            longPressedFlag = true;
            return 2;
        }
    }
    return 0;
}

void Button::loop()
{
    button.loop();
}

//Pass through function
void Button::setDebounceTime(unsigned long time)
{
    button.setDebounceTime(time);
}