#include <Arduino.h>
#include "v1/StateMachine.h"

static StateMachine statemachine;

void setup() {}

void loop() {
    statemachine.update();
}
