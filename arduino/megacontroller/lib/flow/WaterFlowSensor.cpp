#include "WaterFlowSensor.h"

WaterFlowSensor::WaterFlowSensor(uint8_t signalPin, float pulsesPerLitre)
    : _signalPin(signalPin),
      _pulsesPerLitre(pulsesPerLitre),
      _active(false),
      _lastPinState(false),
      _pulseCount(0),
      _lastPulseCount(0),
      _lastCalculationMs(0),
      _flowRateLitresPerMinute(0.0f),
      _totalLitres(0.0f)
{
}

void WaterFlowSensor::begin() {
    pinMode(_signalPin, INPUT_PULLUP);

    _lastPinState = digitalRead(_signalPin);
    _lastCalculationMs = millis();
}

void WaterFlowSensor::update() {
    if (!_active) {
        _lastPinState = digitalRead(_signalPin);
        return;
    }

    bool currentPinState = digitalRead(_signalPin);

    // Count rising edge
    if (!_lastPinState && currentPinState) {
        _pulseCount++;
    }

    _lastPinState = currentPinState;
}

void WaterFlowSensor::calculate() {
    unsigned long now = millis();
    unsigned long elapsedMs = now - _lastCalculationMs;

    if (elapsedMs == 0) {
        return;
    }

    unsigned long pulsesSinceLastCalculation = _pulseCount - _lastPulseCount;

    float litresSinceLastCalculation =
        pulsesSinceLastCalculation / _pulsesPerLitre;

    float elapsedMinutes = elapsedMs / 60000.0f;

    if (_active && elapsedMinutes > 0.0f) {
        _flowRateLitresPerMinute =
            litresSinceLastCalculation / elapsedMinutes;
    } else {
        _flowRateLitresPerMinute = 0.0f;
    }

    _totalLitres += litresSinceLastCalculation;

    _lastPulseCount = _pulseCount;
    _lastCalculationMs = now;
}

bool WaterFlowSensor::shouldCalculate(unsigned long intervalMs) const {
    return millis() - _lastCalculationMs >= intervalMs;
}

float WaterFlowSensor::getFlowRateLitresPerMinute() const {
    return _flowRateLitresPerMinute;
}

float WaterFlowSensor::getTotalLitres() const {
    return _totalLitres;
}

unsigned long WaterFlowSensor::getPulseCount() const {
    return _pulseCount;
}

void WaterFlowSensor::resetTotal() {
    _pulseCount = 0;
    _lastPulseCount = 0;
    _totalLitres = 0.0f;
    _flowRateLitresPerMinute = 0.0f;
    _lastCalculationMs = millis();
}

void WaterFlowSensor::resetFlowRate() {
    _flowRateLitresPerMinute = 0.0f;
    _lastPulseCount = _pulseCount;
    _lastCalculationMs = millis();
}

void WaterFlowSensor::setActive(bool active) {
    _active = active;

    if (!active) {
        _flowRateLitresPerMinute = 0.0f;
        _lastPulseCount = _pulseCount;
        _lastCalculationMs = millis();
    }

    _lastPinState = digitalRead(_signalPin);
}

bool WaterFlowSensor::isActive() const {
    return _active;
}