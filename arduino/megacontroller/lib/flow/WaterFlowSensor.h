#ifndef FLOW_SENSOR_H
#define FLOW_SENSOR_H

#include <Arduino.h>

class WaterFlowSensor {
public:
    WaterFlowSensor(uint8_t signalPin, float pulsesPerLitre);

    void begin();

    // Must be called as often as possible from loop()
    void update();

    // Calculates flow rate based on collected pulses
    void calculate();

    // Optional: calculate only after a fixed interval
    bool shouldCalculate(unsigned long intervalMs) const;

    float getFlowRateLitresPerMinute() const;
    float getTotalLitres() const;
    unsigned long getPulseCount() const;

    void resetTotal();
    void resetFlowRate();

    void setActive(bool active);
    bool isActive() const;

private:
    uint8_t _signalPin;
    float _pulsesPerLitre;

    bool _active;
    bool _lastPinState;

    unsigned long _pulseCount;
    unsigned long _lastPulseCount;

    unsigned long _lastCalculationMs;

    float _flowRateLitresPerMinute;
    float _totalLitres;
};

#endif