#include "ValveControl.h"

ReservoirInletValve::ReservoirInletValve(uint8_t inletPin, uint8_t topSensorPin, uint8_t bottomSensorPin, unsigned long intervalMs)
  : _inletPin(inletPin),
    _topSensorPin(topSensorPin),
    _bottomSensorPin(bottomSensorPin),
    _previousMillis(0),
    _fillValveOpen(false),
    _debounceIntervalMs(intervalMs),
    _topSensorStableState(HIGH),
    _topSensorLastReading(HIGH),
    _topSensorLastChangeTime(0),
    _bottomSensorStableState(HIGH),
    _bottomSensorLastReading(HIGH),
    _bottomSensorLastChangeTime(0),
    _reservoirEmpty(false)
{
}

ReservoirInletValve::ReservoirInletValve(uint8_t inletPin, unsigned long intervalMs)
  : _inletPin(inletPin),
    _topSensorPin(0xFF),
    _bottomSensorPin(0xFF),
    _previousMillis(0),
    _fillValveOpen(false),
    _debounceIntervalMs(intervalMs),
    _topSensorStableState(HIGH),
    _topSensorLastReading(HIGH),
    _topSensorLastChangeTime(0),
    _bottomSensorStableState(HIGH),
    _bottomSensorLastReading(HIGH),
    _bottomSensorLastChangeTime(0),
    _reservoirEmpty(false)
{
}

void ReservoirInletValve::begin() {
  pinMode(_inletPin, OUTPUT);
  // Only configure sensors if they are configured (not 0xFF)
  if (_topSensorPin != 0xFF && _bottomSensorPin != 0xFF) {
    pinMode(_topSensorPin, INPUT);
    pinMode(_bottomSensorPin, INPUT);
  }
  closeValve();
}

int ReservoirInletValve::debouncedRead(uint8_t pin, int &stableState, int &lastReading, unsigned long &lastChangeTime) {
  unsigned long currentMillis = millis();
  int reading = digitalRead(pin);

  // If reading changed from last reading, start debounce timer
  if (reading != lastReading) {
    lastChangeTime = currentMillis;
    lastReading = reading;
  }
  // If reading has been stable for DEBOUNCE_DELAY_MS, update stable state
  else if (currentMillis - lastChangeTime >= DEBOUNCE_DELAY_MS) {
    stableState = reading;
  }

  return stableState;
}

void ReservoirInletValve::checkReservoirLevel() {
  // Skip if this valve doesn't have sensors configured
  if (_topSensorPin == 0xFF || _bottomSensorPin == 0xFF) {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - _previousMillis < _debounceIntervalMs) {
    return;
  }
  _previousMillis = currentMillis;

  int topValue = debouncedRead(_topSensorPin, _topSensorStableState, _topSensorLastReading, _topSensorLastChangeTime);
  int bottomValue = debouncedRead(_bottomSensorPin, _bottomSensorStableState, _bottomSensorLastReading, _bottomSensorLastChangeTime);

  if (topValue == LOW && bottomValue == HIGH) {
    // Reservoir is empty, open the valve if it's not already open
    _reservoirEmpty = true;
    if (!_fillValveOpen) {
      openValve();
    }
  } else if (topValue == HIGH && bottomValue == LOW) {
    _reservoirEmpty = false;
    // Reservoir is full, close the valve if it's not already closed
    if (_fillValveOpen) {
      closeValve();
    }
  }
}

bool ReservoirInletValve::isReservoirEmpty() const {
  return _reservoirEmpty;
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
