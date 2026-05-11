#include <Arduino.h>
#include <ValveControl.h>
#include <Scale.h>

const short RESERVOIR_INLET_VALVE_PIN = 4;
const short RESERVOIR_BOTTOM_SENSOR_PIN = 2;
const short RESERVOIR_TOP_SENSOR_PIN = 3;
const short DB_INLET_VALVE_PIN = 7;
const short DB_SCALE_DOUT_PIN = 5;
const short DB_SCALE_SCK_PIN = 6;
const short EF_SCALE_DOUT_PIN = 8;
const short EF_SCALE_SCK_PIN = 9;
const short EF_INLET_VALVE_PIN = 10;
const long dbScaleCalibrationFactor = 20000;
const long dbScaleZeroFactor = 0;
const long efScaleCalibrationFactor = 25000;
const long efScaleZeroFactor = 0;

unsigned long currentMillis;
unsigned long mainReservoirStatusMillis;
unsigned long getAuxReservoirWeightMillis;
static const unsigned long auxReservoirWeightInterval = 5000UL; // 5 seconds
static const unsigned long mainReservoirCheckInterval = 10000UL; // 10 seconds
float lastDutchBucketWeight = 0.0;
float lastEbbFlowWeight = 0.0;
float auxReservoirMinWeight = 0.0;
float auxReservoirMaxWeight = 0.0;

ReservoirInletValve mainReservoirInletValve(RESERVOIR_INLET_VALVE_PIN, RESERVOIR_TOP_SENSOR_PIN, RESERVOIR_BOTTOM_SENSOR_PIN);
ReservoirInletValve dutchBucketInletValve(DB_INLET_VALVE_PIN);
ReservoirInletValve ebbFlowInletValve(EF_INLET_VALVE_PIN);
Scale dutchBucketScale(DB_SCALE_DOUT_PIN, DB_SCALE_SCK_PIN, dbScaleCalibrationFactor, dbScaleZeroFactor, auxReservoirMinWeight, auxReservoirMaxWeight, 0, 1);
Scale ebbFlowScale(EF_SCALE_DOUT_PIN, EF_SCALE_SCK_PIN, efScaleCalibrationFactor, efScaleZeroFactor, auxReservoirMinWeight, auxReservoirMaxWeight, 8, 1);

void sendStatus();
void readCommands();
void checkDutchBucketReservoirLevel();
void checkEbbFlowReservoirLevel();

void setup()
{
   Serial.begin(115200);  // USB debug
   Serial2.begin(115200); // ESP32 link
   mainReservoirInletValve.begin();
   dutchBucketInletValve.begin();
   dutchBucketScale.setupScale();
   dutchBucketScale.begin();
   ebbFlowInletValve.begin();
   ebbFlowScale.setupScale();
   ebbFlowScale.begin();
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
   ebbFlowScale.updateCalibration();
   if (currentMillis - getAuxReservoirWeightMillis >= auxReservoirWeightInterval) 
   {
      checkDutchBucketReservoirLevel();
      checkEbbFlowReservoirLevel();
      getAuxReservoirWeightMillis = currentMillis;
   }
}

void checkDutchBucketReservoirLevel()
{
   lastDutchBucketWeight = fabs(dutchBucketScale.getWeight());
   if (lastDutchBucketWeight < auxReservoirMinWeight)
   {
      dutchBucketInletValve.openValve();
   }
   else if (lastDutchBucketWeight >= auxReservoirMaxWeight)
   {
      dutchBucketInletValve.closeValve();
   }
}

void checkEbbFlowReservoirLevel()
{
   lastEbbFlowWeight = fabs(ebbFlowScale.getWeight());
   if (lastEbbFlowWeight < auxReservoirMinWeight)
   {
      ebbFlowInletValve.openValve();
   }
   else if (lastEbbFlowWeight >= auxReservoirMaxWeight)
   {
      ebbFlowInletValve.closeValve();
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
      else if (cmd == "STARTEFSCALECAL")
      {
         Serial2.println("CAL,EbbFlow,start");
         ebbFlowScale.beginCalMode();
      }
      else if (cmd == "STOPEFSCALECAL")
      {
         Serial2.println("CAL,EbbFlow,stop");
         ebbFlowScale.endCalMode();
      }
      else if (cmd.startsWith("AUX_RESERVOIR_CONFIG"))
      {
         String payload = cmd.substring(21); // drop "AUX_RESERVOIR_CONFIG,"
         // Serial.print("Received config payload: ");
         // Serial.println(cmd);
         int i1 = payload.indexOf(',');
         int i2 = payload.indexOf(',', i1 + 1);
         auxReservoirMinWeight = payload.substring(i1 + 1, i2).toInt();
         auxReservoirMaxWeight = payload.substring(i2 + 1).toInt();
      }
   }
}

void sendStatus()
{
   Serial.println("DB weight: " + String(lastDutchBucketWeight) + " kg, EF weight: " + String(lastEbbFlowWeight) + " kg");
   int mainReservoirEmpty = mainReservoirInletValve.isReservoirEmpty() ? 1 : 0;
   int mainInletValveOpen = mainReservoirInletValve.isValveOpen() ? 1 : 0;
   int dutchBucketInletValveOpen = dutchBucketInletValve.isValveOpen() ? 1 : 0;
   int dutchBucketReservoirEmpty = lastDutchBucketWeight < auxReservoirMinWeight ? 1 : 0;
   int ebbFlowInletValveOpen = ebbFlowInletValve.isValveOpen() ? 1 : 0;
   int ebbFlowReservoirEmpty = lastEbbFlowWeight < auxReservoirMinWeight ? 1 : 0;

   Serial2.print("STAT,M_R_Empty=");
   Serial2.print(mainReservoirEmpty);
   Serial2.print(",MI_IV_O=");
   Serial2.print(mainInletValveOpen);
   Serial2.print(",DB_Weight=");
   Serial2.print(lastDutchBucketWeight);
   Serial2.print(",DB_IV_O=");
   Serial2.print(dutchBucketInletValveOpen);
   Serial2.print(",DB_R_Empty=");
   Serial2.print(dutchBucketReservoirEmpty);
   Serial2.print(",EF_Weight=");
   Serial2.print(lastEbbFlowWeight);
   Serial2.print(",EF_IV_O=");
   Serial2.print(ebbFlowInletValveOpen);
   Serial2.print(",EF_R_Empty=");
   Serial2.println(ebbFlowReservoirEmpty);

}