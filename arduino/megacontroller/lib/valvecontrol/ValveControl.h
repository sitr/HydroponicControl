#ifndef VALVECONTROL_H
#define VALVECONTROL_H

#include <Arduino.h>

class ReservoirInletValve {
  public:
    ReservoirInletValve(uint8_t inletPin, uint8_t topSensorPin, uint8_t bottomSensorPin, unsigned long intervalMs = 50UL);
    ReservoirInletValve(uint8_t inletPin, unsigned long intervalMs = 50UL);
    void begin();
    void checkReservoirLevel();
    bool isValveOpen() const;
    bool isReservoirEmpty() const;
    String getReservoirStatus() const;
    void openValve();
    void closeValve();

  private:
    uint8_t _inletPin;
    uint8_t _topSensorPin;
    uint8_t _bottomSensorPin;
    unsigned long _previousMillis;
    bool _fillValveOpen;
    bool _reservoirEmpty;
    unsigned long _debounceIntervalMs;

    // Debouncing variables for top sensor
    int _topSensorStableState;
    int _topSensorLastReading;
    unsigned long _topSensorLastChangeTime;

    // Debouncing variables for bottom sensor
    int _bottomSensorStableState;
    int _bottomSensorLastReading;
    unsigned long _bottomSensorLastChangeTime;

    static const unsigned long DEBOUNCE_DELAY_MS = 50UL;

    int debouncedRead(uint8_t pin, int &stableState, int &lastReading, unsigned long &lastChangeTime);
};

#endif
