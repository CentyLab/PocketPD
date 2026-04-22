#include <Arduino.h>
#include <v1/StateMachine.h>

StateMachine statemachine;

void setup()
{
}

void loop()
{
    statemachine.update();
}
