#include <Arduino.h>
#include <HX711.h>

class ReservoirScale {
   public:
      ReservoirScale(int doutPin, int sckPin, long calibrationFactor, long zeroFactor, int minWeight, int maxWeight, int eepromAddress = 0, int weightSign = -1);
      void begin();
      void beginCalMode();
      void endCalMode();
      void calibrateScale();
      void updateCalibration();
      void setupScale();
      float getWeight();
      bool isCalModeActive();
      void setCalibrationFactor(long calibrationFactor);

   private:
      HX711 hx711_scale;
      bool scaleCalMode;
      unsigned long scaleCalMillis;
      static const unsigned long scaleCalPeriod = 500;
      int _cfAddress;
      int _zfAddress;
      int _weightSign;
      int _minWeight;
      int _maxWeight;

      // Non-blocking calibration state machine
      enum CalibrationState {
         CAL_IDLE,
         CAL_STARTUP_DELAY,
         CAL_REMOVE_WEIGHT_COUNTDOWN,
         CAL_TARING_DELAY,
         CAL_ZERO_WEIGHT_COUNTDOWN,
         CAL_ZERO_APPLIED_DELAY,
         CAL_ACTIVE
      };
      
      CalibrationState calState;
      unsigned long calStateStartTime;
      int calCountdown;
      static const unsigned long CAL_STARTUP_DELAY_MS = 1500;
      static const unsigned long CAL_TARING_DELAY_MS = 2000;
      static const unsigned long CAL_ZERO_APPLIED_DELAY_MS = 1500;
      static const unsigned long CAL_COUNTDOWN_STEP_MS = 1000;

      // Calibration values
      struct hx711CalVals 
      {
         long calibrationFactor;
         long zeroFactor;
         int scaleDoutPin;
         int scaleSckPin;
      };
      hx711CalVals _mixingRes;
};