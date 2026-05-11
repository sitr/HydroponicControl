#include "Scale.h"
#include <HX711.h>
#include <EEPROM.h>

Scale::Scale(int doutPin, int sckPin, long calibrationFactor, long zeroFactor, int minWeight, int maxWeight, int eepromAddress, int weightSign)
  : _cfAddress(eepromAddress),
    _zfAddress(eepromAddress + 4),
    _weightSign(weightSign),
    _minWeight(minWeight),
    _maxWeight(maxWeight),
    scaleCalMode(false)
{
   _mixingRes.scaleDoutPin = doutPin;
   _mixingRes.scaleSckPin = sckPin;
   _mixingRes.calibrationFactor = calibrationFactor;
   _mixingRes.zeroFactor = zeroFactor;
}

void Scale::begin()
{
   scale.begin(_mixingRes.scaleDoutPin, _mixingRes.scaleSckPin);
   setupScale();
}

float Scale::getWeight()
{
   float weight = scale.get_units(5) * _weightSign;
   return weight;
}

bool Scale::isCalModeActive()
{
   return scaleCalMode;
}

void Scale::setupScale()
{
   EEPROM.get(_cfAddress, _mixingRes.calibrationFactor);
   EEPROM.get(_zfAddress, _mixingRes.zeroFactor);
   char buff[80];
   sprintf(buff, "<HX711: Loaded calibration factor of %ld and zero factor of %ld>", _mixingRes.calibrationFactor, _mixingRes.zeroFactor);
   Serial.println(buff);
   Serial.println(buff);
   scale.set_scale(_mixingRes.calibrationFactor); // Assign calibration factor from calibration process below
   delay(500);
   scale.set_offset(_mixingRes.zeroFactor); // Zero out the scale using zero_factor from calibration process below
}

void Scale::beginCalMode()
{
   char buff[80];
   scaleCalMillis = millis();
   Serial.println("<HX711: Calibration starting.>");
   Serial2.println("CAL,DutchBucket,start");
   delay(1500);
   for (int i = 10; i >= 0; i--)
   {
      sprintf(buff, "<HX711: Remove ALL weight from sensors. You have %d seconds...>", i);
      Serial.println(buff);
      delay(1000);
   }
   Serial.println("<HX711: Taring scale...>");
   Serial2.println("CAL,DutchBucket,taring");
   delay(2000);
   scale.set_scale();
   scale.tare(); // Reset the scale to 0
   _mixingRes.calibrationFactor = -20000;
   for (int i = 10; i >= 0; i--)
   {
      sprintf(buff, "<HX711: Place stuff to be zeroed out on scale now! You have %d seconds...>", i);
      Serial.println(buff);
      delay(1000);
   }
   Serial.println("<HX711: Zero factor applied. Add known weight now and adjust calibration factor.>");
   Serial2.println("CAL,DutchBucket,zero_applied");
   _mixingRes.zeroFactor = scale.read_average(); // Get a baseline reading
   Serial.print("zero factor: ");
   Serial.println(_mixingRes.zeroFactor);
   scale.set_offset(_mixingRes.zeroFactor);
   delay(1500);
   scaleCalMode = true;
   Serial2.println("CAL,DutchBucket,active");
}

void Scale::endCalMode()
{
   scaleCalMode = false;
   EEPROM.put(_cfAddress, _mixingRes.calibrationFactor);
   EEPROM.put(_zfAddress, _mixingRes.zeroFactor); 
   Serial.println("<HX711: Calibration saved to EEPROM!>");
   Serial.println(_mixingRes.calibrationFactor);
   Serial.println(_mixingRes.zeroFactor);
   Serial2.println("CAL,DutchBucket,stop");
   scale.tare();
}

void Scale::calibrateScale()
{
   scale.set_scale(_mixingRes.calibrationFactor); // Adjust the calibration factor
   Serial.print("<HX711: ");
   Serial.print(scale.get_units(), 1);
   Serial.print("kg, ");
   Serial.print("Calibration factor: ");
   Serial.print(_mixingRes.calibrationFactor);
   Serial.println('>');
}

void Scale::updateCalibration()
{
   unsigned long now = millis();
   if (scaleCalMode && (now - scaleCalMillis >= scaleCalPeriod))
   {
      calibrateScale();
      // Send calibration weight reading to ESP32/HomeAssistant
      Serial2.print("CAL,DutchBucket,weight=");
      Serial2.println(scale.get_units(), 1);
      scaleCalMillis = now;
   }
}