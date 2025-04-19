#include <Arduino.h>
#include <StateMachine.h>

StateMachine statemachine;

void setup()
{
}

void loop()
{
    statemachine.update();
}

