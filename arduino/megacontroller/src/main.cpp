#include <Arduino.h>
#include <ValveControl.h>

const short reservoirInletValvePin = 4;
const short bottomSensorPin = 2;
const short topSensorPin = 3;

ReservoirInletValve inletValve(reservoirInletValvePin, topSensorPin, bottomSensorPin);

void setup() {
  inletValve.begin();
}

void loop() {
  inletValve.checkReservoirLevel();
}

