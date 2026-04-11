#ifndef VALVECONTROL_H
#define VALVECONTROL_H

#include <Arduino.h>

class ReservoirInletValve {
  public:
    ReservoirInletValve(uint8_t inletPin, uint8_t topSensorPin, uint8_t bottomSensorPin, unsigned long intervalMs = 5000UL);
    void begin();
    void checkReservoirLevel();
    bool isValveOpen() const;

  private:
    uint8_t _inletPin;
    uint8_t _topSensorPin;
    uint8_t _bottomSensorPin;
    unsigned long _previousMillis;
    bool _fillValveOpen;
    unsigned long _intervalMs;

    void openValve();
    void closeValve();
};

#endif
