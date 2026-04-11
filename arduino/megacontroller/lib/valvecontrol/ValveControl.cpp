#include "ValveControl.h"

ReservoirInletValve::ReservoirInletValve(uint8_t inletPin, uint8_t topSensorPin, uint8_t bottomSensorPin, unsigned long intervalMs)
  : _inletPin(inletPin),
    _topSensorPin(topSensorPin),
    _bottomSensorPin(bottomSensorPin),
    _previousMillis(0),
    _fillValveOpen(false),
    _intervalMs(intervalMs)
{
}

void ReservoirInletValve::begin() {
  pinMode(_inletPin, OUTPUT);
  pinMode(_topSensorPin, INPUT_PULLUP);
  pinMode(_bottomSensorPin, INPUT_PULLUP);
  closeValve();
}

void ReservoirInletValve::checkReservoirLevel() {
  unsigned long currentMillis = millis();
  if (currentMillis - _previousMillis < _intervalMs) {
    return;
  }

  int topValue = digitalRead(_topSensorPin);
  delay(200);
  int bottomValue = digitalRead(_bottomSensorPin);

  if (topValue == LOW && bottomValue == HIGH) {
    if (_fillValveOpen) {
      closeValve();
    }
  } else if (topValue == HIGH && bottomValue == LOW) {
    if (!_fillValveOpen) {
      openValve();
    }
  }

  _previousMillis += _intervalMs;
}

bool ReservoirInletValve::isValveOpen() const {
  return _fillValveOpen;
}

void ReservoirInletValve::openValve() {
  digitalWrite(_inletPin, LOW);
  _fillValveOpen = true;
}

void ReservoirInletValve::closeValve() {
  digitalWrite(_inletPin, HIGH);
  _fillValveOpen = false;
}
