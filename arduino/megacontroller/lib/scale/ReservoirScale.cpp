#include "ReservoirScale.h"
#include <HX711.h>
#include <EEPROM.h>

ReservoirScale::ReservoirScale(int doutPin, int sckPin, long calibrationFactor, long zeroFactor, int minWeight, int maxWeight, int eepromAddress, int weightSign)
  : hx711_scale(),
    scaleCalMode(false),
    scaleCalMillis(0),
    _cfAddress(eepromAddress),
    _zfAddress(eepromAddress + 4),
    _weightSign(weightSign),
    _minWeight(minWeight),
    _maxWeight(maxWeight),
    calState(CAL_IDLE),
    calStateStartTime(0),
    calCountdown(0)
{
   _mixingRes.scaleDoutPin = doutPin;
   _mixingRes.scaleSckPin = sckPin;
   _mixingRes.calibrationFactor = calibrationFactor;
   _mixingRes.zeroFactor = zeroFactor;
   pinMode(_mixingRes.scaleSckPin, OUTPUT);
   pinMode(_mixingRes.scaleDoutPin, INPUT);
   digitalWrite(_mixingRes.scaleDoutPin, LOW);
   digitalWrite(_mixingRes.scaleSckPin, LOW);
}

void ReservoirScale::begin()
{
   setupScale();
   hx711_scale.begin(_mixingRes.scaleDoutPin, _mixingRes.scaleSckPin);
   Serial.println("doutPin = " + String(_mixingRes.scaleDoutPin) + ", sckPin = " + String(_mixingRes.scaleSckPin));
   if (!hx711_scale.is_ready()) {
      Serial.println("HX711 er IKKE klar. Sjekk kabling/pinner.");
      Serial.println(digitalRead(_mixingRes.scaleDoutPin));
      while (1);
   }

   Serial.println("HX711 ready.");
}

float ReservoirScale::getWeight()
{
   // Avoid blocking if the HX711 is not connected or not ready.
   // The HX711 library normally waits for data ready, which can stall loop().
   if (!hx711_scale.wait_ready_timeout(50)) {
      return 0.0f;
   }

   float weight = hx711_scale.get_units(5) * _weightSign;
   return weight;
}

bool ReservoirScale::isCalModeActive()
{
   return scaleCalMode;
}

void ReservoirScale::setCalibrationFactor(long calibrationFactor)
{
   _mixingRes.calibrationFactor = calibrationFactor;
}

void ReservoirScale::setupScale()
{
   EEPROM.get(_cfAddress, _mixingRes.calibrationFactor);
   EEPROM.get(_zfAddress, _mixingRes.zeroFactor);
   char buff[80];
   sprintf(buff, "<HX711: Loaded calibration factor of %ld and zero factor of %ld>", _mixingRes.calibrationFactor, _mixingRes.zeroFactor);
   Serial.println(buff);
   Serial.println(buff);
   hx711_scale.set_scale(_mixingRes.calibrationFactor); // Assign calibration factor magnitude from calibration process below
   delay(500);
   hx711_scale.set_offset(_mixingRes.zeroFactor); // Zero out the scale using zero_factor from calibration process below
}

void ReservoirScale::beginCalMode()
{
   Serial.println("<HX711: Calibration starting.>");
   Serial2.println("CAL,SCALE,START");
   
   // Initialize non-blocking state machine
   scaleCalMode = true;
   calState = CAL_STARTUP_DELAY;
   calStateStartTime = millis();
   calCountdown = 10;
}

void ReservoirScale::endCalMode()
{
   scaleCalMode = false;
   calState = CAL_IDLE;
   EEPROM.put(_cfAddress, _mixingRes.calibrationFactor);
   EEPROM.put(_zfAddress, _mixingRes.zeroFactor); 
   Serial.println("<HX711: Calibration saved to EEPROM!>");
   Serial.println(_mixingRes.calibrationFactor);
   Serial.println(_mixingRes.zeroFactor);
   hx711_scale.tare();
   Serial2.println("CAL,SCALE,STOP");
}

void ReservoirScale::calibrateScale()
{
   hx711_scale.set_scale(fabs(static_cast<float>(_mixingRes.calibrationFactor))); // Adjust the calibration factor using magnitude
   if (hx711_scale.wait_ready_timeout(1000)) {
      long raw = hx711_scale.read_average(10);
      Serial.print("<HX711 RAW: ");
      Serial.print(raw);
      Serial.print(" offset=");
      Serial.print(hx711_scale.get_offset());
      Serial.print(" scale=");
      Serial.print(hx711_scale.get_scale());
      Serial.println(">"
      );
   } else {
      Serial.println("<HX711 RAW: not ready>");
   }
   float weight = hx711_scale.get_units(10) * _weightSign;
   Serial.print("<HX711: ");
   Serial.print(weight, 1);
   Serial.print("kg, ");
   Serial.print("Calibration factor: ");
   Serial.print(_mixingRes.calibrationFactor);
   Serial.println('>');
}

void ReservoirScale::updateCalibration()
{
   unsigned long now = millis();
   
   if (!scaleCalMode)
      return;

   // Handle state machine
   switch (calState)
   {
      case CAL_IDLE:
         break;

      case CAL_STARTUP_DELAY:
         if (now - calStateStartTime >= CAL_STARTUP_DELAY_MS)
         {
            calState = CAL_REMOVE_WEIGHT_COUNTDOWN;
            calStateStartTime = now;
            calCountdown = 9;
            char buff[80];
            sprintf(buff, "<HX711: Remove ALL weight from sensors. You have %d seconds...>", 10);
            Serial.println(buff);
         }
         break;

      case CAL_REMOVE_WEIGHT_COUNTDOWN:
      {
         if (now - calStateStartTime >= CAL_COUNTDOWN_STEP_MS)
         {
            char buff[80];
            sprintf(buff, "<HX711: Remove ALL weight from sensors. You have %d seconds...>", calCountdown);
            Serial.println(buff);
            calCountdown--;
            calStateStartTime = now;

            if (calCountdown < 0)
            {
               Serial.println("<HX711: Taring scale...>");
               Serial2.println("CAL,SCALE,TARING");
               calState = CAL_TARING_DELAY;
               calStateStartTime = now;
            }
         }
         break;
      }

      case CAL_TARING_DELAY:
         if (now - calStateStartTime >= CAL_TARING_DELAY_MS)
         {
            hx711_scale.set_scale();
            hx711_scale.tare(); // Reset the scale to 0
            _mixingRes.calibrationFactor = -20000;
            
            calState = CAL_ZERO_WEIGHT_COUNTDOWN;
            calStateStartTime = now;
            calCountdown = 9;
            {
               char buff[80];
               sprintf(buff, "<HX711: Place stuff to be zeroed out on scale now! You have %d seconds...>", 10);
               Serial.println(buff);
            }
         }
         break;

      case CAL_ZERO_WEIGHT_COUNTDOWN:
      {
         if (now - calStateStartTime >= CAL_COUNTDOWN_STEP_MS)
         {
            char buff[80];
            sprintf(buff, "<HX711: Place stuff to be zeroed out on scale now! You have %d seconds...>", calCountdown);
            Serial.println(buff);
            calCountdown--;
            calStateStartTime = now;

            if (calCountdown < 0)
            {
               Serial.println("<HX711: Zero factor applied. Add known weight now and adjust calibration factor.>");
               Serial2.println("CAL,SCALE,ZERO_APPLIED");
               _mixingRes.zeroFactor = hx711_scale.read_average(); // Get a baseline reading
               Serial.print("zero factor: ");
               Serial.println(_mixingRes.zeroFactor);
               hx711_scale.set_offset(_mixingRes.zeroFactor);
               
               calState = CAL_ZERO_APPLIED_DELAY;
               calStateStartTime = now;
            }
         }
         break;
      }

      case CAL_ZERO_APPLIED_DELAY:
         if (now - calStateStartTime >= CAL_ZERO_APPLIED_DELAY_MS)
         {
            calState = CAL_ACTIVE;
            scaleCalMillis = now;
            Serial2.println("CAL,SCALE,ACTIVE");
         }
         break;

      case CAL_ACTIVE:
         // Update weight reading periodically during calibration
         if (now - scaleCalMillis >= scaleCalPeriod)
         {
            calibrateScale();
            // Send calibration weight reading to ESP32/HomeAssistant
            hx711_scale.set_scale(fabs(static_cast<float>(_mixingRes.calibrationFactor)));
            float currentWeight = hx711_scale.get_units(10) * _weightSign;
            Serial2.print("CAL,SCALE,WEIGHT=");
            Serial2.println(currentWeight, 2);
            scaleCalMillis = now;
         }
         break;
   }
}