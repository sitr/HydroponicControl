#include <Arduino.h>
#include <HX711.h>

class Scale {
   public:
      Scale(int doutPin, int sckPin, long calibrationFactor, long zeroFactor, int minWeight, int maxWeight, int eepromAddress = 0, int weightSign = -1);
      void begin();
      void beginCalMode();
      void endCalMode();
      void calibrateScale();
      void updateCalibration();
      void setupScale();
      float getWeight();
      bool isCalModeActive();

   private:
      HX711 scale;
      bool scaleCalMode;
      unsigned long scaleCalMillis;
      static const unsigned long scaleCalPeriod = 500;
      int _cfAddress;
      int _zfAddress;
      int _weightSign;
      int _minWeight;
      int _maxWeight;

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