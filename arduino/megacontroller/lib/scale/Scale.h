#include <Arduino.h>
#include <HX711.h>

class Scale {
   public:
      Scale(int doutPin, int sckPin, long calibrationFactor, long zeroFactor, int minWeight, int maxWeight);
      void begin();
      void beginCalMode();
      void endCalMode();
      void calibrateScale();
      void updateCalibration();
      void setupScale();
      float getWeight();

   private:
      HX711 scale;
      bool scaleCalMode;
      unsigned long scaleCalMillis;
      static const unsigned long scaleCalPeriod = 500;
      int cfAddress = 0;
      int zfAddress = 4;
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