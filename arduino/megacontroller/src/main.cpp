#include <Arduino.h>
#include <ValveControl.h>

const short reservoirInletValvePin = 5;
const short bottomSensorPin = 22;
const short topSensorPin = 23;

ReservoirInletValve inletValve(reservoirInletValvePin, topSensorPin, bottomSensorPin);

void setup() {
  inletValve.begin();
}

void loop() {
  inletValve.checkReservoirLevel();
}

