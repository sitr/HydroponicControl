#include <Arduino.h>
#include <ValveControl.h>
#include <Scale.h>


const short RESERVOIR_INLET_VALVE_PIN = 4;
const short DB_INLET_VALVE_PIN = 7;
const short BOTTOM_SENSOR_PIN = 2;
const short TOP_SENSOR_PIN = 3;
const short DB_SCALE_DOUT_PIN = 5;
const short DB_SCALE_SCK_PIN = 6;
const long scaleCalibrationFactor = 20000;
const long scaleZeroFactor = 0;
unsigned long currentMillis;
unsigned long mainReservoirStatusMillis;
unsigned long getAuxReservoirWeightMillis;
static const unsigned long auxReservoirWeightInterval = 5000UL; // 5 seconds
static const unsigned long mainReservoirCheckInterval = 60000UL; // 1 minute
float lastDutchBucketWeight = 0.0;
float auxReservoirMinWeight = 0.0;
float auxReservoirMaxWeight = 0.0;

ReservoirInletValve mainReservoirInletValve(RESERVOIR_INLET_VALVE_PIN, TOP_SENSOR_PIN, BOTTOM_SENSOR_PIN);
ReservoirInletValve dutchBucketInletValve(DB_INLET_VALVE_PIN);
Scale dutchBucketScale(DB_SCALE_DOUT_PIN, DB_SCALE_SCK_PIN, scaleCalibrationFactor, scaleZeroFactor, auxReservoirMinWeight, auxReservoirMaxWeight);

void sendStatus();
void readCommands();

void setup()
{
   Serial.begin(115200);  // USB debug
   Serial2.begin(115200); // ESP32 link
   mainReservoirInletValve.begin();
   dutchBucketInletValve.begin();
   dutchBucketScale.setupScale();
   dutchBucketScale.begin();
}

void loop()
{
   currentMillis = millis();
   if (currentMillis - mainReservoirStatusMillis >= mainReservoirCheckInterval)
   {
      mainReservoirStatusMillis = currentMillis;
      mainReservoirInletValve.checkReservoirLevel();
   }
   readCommands();
   dutchBucketScale.updateCalibration();
   if (currentMillis - getAuxReservoirWeightMillis >= auxReservoirWeightInterval) 
   {
      checkDutchBucketReservoirLevel();
      getAuxReservoirWeightMillis = currentMillis;
   }
}

void checkDutchBucketReservoirLevel()
{
   lastDutchBucketWeight = dutchBucketScale.getWeight();
   if (lastDutchBucketWeight < 0)
   {
      lastDutchBucketWeight = 0.0; // Ensure weight doesn't go negative
   }
   if (lastDutchBucketWeight < auxReservoirMinWeight)
   {
      dutchBucketInletValve.openValve();
   }
   else if (lastDutchBucketWeight >= auxReservoirMaxWeight)
   {
      dutchBucketInletValve.closeValve();
   }
}

void readCommands()
{
   if (Serial2.available())
   {
      String cmd = Serial2.readStringUntil('\n');
      cmd.trim();

      if (cmd == "STATUS")
      {
         sendStatus();
      }
      else if (cmd == "OPENMAINVALVE")
      {
         mainReservoirInletValve.openValve();
      }
      else if (cmd == "CLOSEMAINVALVE")
      {
         mainReservoirInletValve.closeValve();
      }
      else if (cmd == "STARTDBSCALECAL")
      {
         Serial2.println("CAL,DutchBucket,start");
         dutchBucketScale.beginCalMode();
      }
      else if (cmd == "STOPDBSCALECAL")
      {
         Serial2.println("CAL,DutchBucket,stop");
         dutchBucketScale.endCalMode();
      }
      else if (cmd.startsWith("DB_RESERVOIR_CONFIG"))
      {
         String payload = cmd.substring(20); // drop "DB_RESERVOIR_CONFIG,"
         Serial.print("Received config payload: ");
         Serial.println(cmd);
         int i1 = payload.indexOf(',');
         int i2 = payload.indexOf(',', i1 + 1);
         auxReservoirMinWeight = payload.substring(i1 + 1, i2).toInt();
         auxReservoirMaxWeight = payload.substring(i2 + 1).toInt();
      }
   }
}

void sendStatus()
{
   int mainReservoirEmpty = mainReservoirInletValve.isReservoirEmpty() ? 1 : 0;
   int mainInletValveOpen = mainReservoirInletValve.isValveOpen() ? 1 : 0;
   int dutchBucketInletValveOpen = dutchBucketInletValve.isValveOpen() ? 1 : 0;
   int dutchBucketReservoirEmpty = lastDutchBucketWeight < auxReservoirMinWeight ? 1 : 0;

   Serial2.print("STAT,mainReservoirEmpty=");
   Serial2.print(mainReservoirEmpty);
   Serial2.print(",mainInletValveOpen=");
   Serial2.print(mainInletValveOpen);
   Serial2.print(",dbReservoirWeight=");
   Serial2.print(lastDutchBucketWeight);
   Serial2.print(",dutchBucketInletValveOpen=");
   Serial2.print(dutchBucketInletValveOpen);
   Serial2.print(",dutchBucketReservoirEmpty=");
   Serial2.println(dutchBucketReservoirEmpty);
}