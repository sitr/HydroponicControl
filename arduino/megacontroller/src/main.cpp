#include <Arduino.h>
#include <ValveControl.h>
#include <ReservoirScale.h>
#include <PwmControl.h>
#include <NutrientPumpController.h>
#include <WaterFlowSensor.h>

//PWM
const short FLORA_GROW_PUMP_PIN = 2;
const short FLORA_MICRO_PUMP_PIN = 3;
const short FLORA_BLOOM_PUMP_PIN = 4;
const short PH_DOWN_PUMP_PIN = 5;
//Digital
const short RESERVOIR_FLOW_SENSOR_PIN = 19; // FS300A flow sensor input pin
const short RESERVOIR_BOTTOM_SENSOR_PIN = 42;
const short RESERVOIR_TOP_SENSOR_PIN = 43;

const short DB_SCALE_DOUT_PIN = 24;
const short DB_SCALE_SCK_PIN = 25;
const short EF_SCALE_DOUT_PIN = 40;
const short EF_SCALE_SCK_PIN = 41;

const short RESERVOIR_INLET_VALVE_PIN = 28;
const short DB_INLET_VALVE_PIN = 29; // Pin 2 on relay board
const short EF_INLET_VALVE_PIN = 30; // Pin 3 on relay board
const short DB_NUTRIENT_VALVE_PIN = 31; // Pin 4 on relay board
const short EF_NUTRIENT_VALVE_PIN = 32; // Pin 5 on relay board

const float RESERVOIR_FLOW_PULSES_PER_LITRE = 60.0f;

const long dbScaleCalibrationFactor = 20000;
const long dbScaleZeroFactor = 0;
const float efScaleCalibrationFactor = 400.0;
const long efScaleZeroFactor = 0;
const unsigned long MAX_NUTRIENT_PUMP_RUN_MS = 60000UL; // 60 seconds

unsigned long currentMillis;
unsigned long mainReservoirStatusMillis;
unsigned long getAuxReservoirWeightMillis;
static const unsigned long auxReservoirWeightInterval = 5000UL;  // 5 seconds
static const unsigned long mainReservoirCheckInterval = 3000UL; // 10 seconds
float lastDutchBucketWeight = 0.0;
float lastEbbFlowWeight = 0.0;
float auxReservoirMinWeight = 0.0;
float auxReservoirMaxWeight = 0.0;
float mainInletValveflowRate = 0.0f;

PwmControl floraGrowPump(FLORA_GROW_PUMP_PIN, false);
PwmControl floraMicroPump(FLORA_MICRO_PUMP_PIN, false);
PwmControl floraBloomPump(FLORA_BLOOM_PUMP_PIN, false);
PwmControl phDownPump(PH_DOWN_PUMP_PIN, false);

NutrientPumpController nutrientPumps(floraGrowPump, floraMicroPump, floraBloomPump, phDownPump, Serial);

ReservoirInletValve mainReservoirInletValve(RESERVOIR_INLET_VALVE_PIN, RESERVOIR_TOP_SENSOR_PIN, RESERVOIR_BOTTOM_SENSOR_PIN);
ReservoirInletValve dutchBucketInletValve(DB_INLET_VALVE_PIN);
ReservoirInletValve ebbFlowInletValve(EF_INLET_VALVE_PIN);
ReservoirScale dutchBucketScale(DB_SCALE_DOUT_PIN, DB_SCALE_SCK_PIN, dbScaleCalibrationFactor, dbScaleZeroFactor, auxReservoirMinWeight, auxReservoirMaxWeight, 0, 1);
ReservoirScale ebbFlowScale(EF_SCALE_DOUT_PIN, EF_SCALE_SCK_PIN, efScaleCalibrationFactor, efScaleZeroFactor, auxReservoirMinWeight, auxReservoirMaxWeight, 8, 1);
WaterFlowSensor reservoirFlowSensor(RESERVOIR_FLOW_SENSOR_PIN, RESERVOIR_FLOW_PULSES_PER_LITRE);
void sendStatus();
void readCommands();
void checkDutchBucketReservoirLevel();
void checkEbbFlowReservoirLevel();

void setup()
{
   Serial.begin(115200);  // USB debug
   Serial2.begin(9600); // ESP32 link
   Serial2.setTimeout(50);

   floraGrowPump.begin();
   floraMicroPump.begin();
   floraBloomPump.begin();
   phDownPump.begin();
   nutrientPumps.begin();

   mainReservoirInletValve.begin();
   dutchBucketInletValve.begin();
   //dutchBucketScale.begin();
   ebbFlowInletValve.begin();
   ebbFlowScale.begin();
   reservoirFlowSensor.begin();
}

void loop()
{
   currentMillis = millis();
   reservoirFlowSensor.update();
   
   reservoirFlowSensor.setActive(mainReservoirInletValve.isValveOpen());
   if (reservoirFlowSensor.shouldCalculate(mainReservoirCheckInterval)) {
        reservoirFlowSensor.calculate();
   }
   if (currentMillis - mainReservoirStatusMillis >= mainReservoirCheckInterval)
   {
      mainReservoirStatusMillis = currentMillis;
      mainReservoirInletValve.checkReservoirLevel();
      mainInletValveflowRate = reservoirFlowSensor.getFlowRateLitresPerMinute();

      if (!mainReservoirInletValve.isValveOpen()) {
         mainInletValveflowRate = 0.0f;
      }
   }

   readCommands();
   //dutchBucketScale.updateCalibration();
   ebbFlowScale.updateCalibration();
   
   if (currentMillis - getAuxReservoirWeightMillis >= auxReservoirWeightInterval)
   {
      //checkDutchBucketReservoirLevel();
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

int nutrientPumpNameToId(const String &nutrientPump)
{
   if (nutrientPump == "FLORA_GROW")
      return FLORA_GROW_PUMP;

   if (nutrientPump == "FLORA_MICRO")
      return FLORA_MICRO_PUMP;

   if (nutrientPump == "FLORA_BLOOM")
      return FLORA_BLOOM_PUMP;
   if (nutrientPump == "PH_DOWN")
      return PH_DOWN_PUMP;
   return -1;
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
      else if (cmd == "OPEN_MAIN_VALVE")
      {
         mainReservoirInletValve.openValve();
      }
      else if (cmd == "CLOSE_MAIN_VALVE")
      {
         mainReservoirInletValve.closeValve();
      }
      else if (cmd == "OPEN_DB_VALVE")
      {
         dutchBucketInletValve.openValve();
      }
      else if (cmd == "CLOSE_DB_VALVE")
      {
         dutchBucketInletValve.closeValve();
      }
      else if (cmd == "OPEN_EF_VALVE")
      {
         ebbFlowInletValve.openValve();
      }
      else if (cmd == "CLOSE_EF_VALVE")
      {
         ebbFlowInletValve.closeValve();
      }
      else if (cmd == "START_DB_SCALECAL")
      {
         dutchBucketScale.beginCalMode();
      }
      else if (cmd == "STOP_DB_SCALECAL")
      {
         dutchBucketScale.endCalMode();
      }
      else if (cmd == "START_EF_SCALECAL")
      {
         ebbFlowScale.beginCalMode();
      }
      else if (cmd.startsWith("CAL_FACTOR_EF_SCALE"))
      {
         String payload = cmd.substring(20); // drop "CAL_FACTOR_EF_SCALE,"
         Serial.println("Setting EF scale calibration factor to: " + payload);
         long calibrationFactor = payload.toInt();
         ebbFlowScale.setCalibrationFactor(calibrationFactor);
      }
       else if (cmd == "STOP_EF_SCALECAL")
      {
         ebbFlowScale.endCalMode();
      }
      else if (cmd.startsWith("CAL_NUTRIENT_PUMP"))
      {
         Serial.println(cmd);
         String payload = cmd.substring(18); // drop "CAL_NUTRIENT_PUMP,"
         int i1 = payload.indexOf(',');
         int i2 = payload.indexOf(',', i1 + 1);

         String nutrientPumpName = payload.substring(0, i1);
         unsigned long durationMs = payload.substring(i1 + 1, i2).toInt();
         int pumpSpeed = payload.substring(i2 + 1).toInt();
         int pumpId = nutrientPumpNameToId(nutrientPumpName);
         Serial.print("Calibrating pump: ");
         Serial.print(nutrientPumpName);
         Serial.print(" for (ms): ");
         Serial.print(durationMs);
         Serial.print(" at speed: ");
         Serial.println(pumpSpeed);

         if (pumpId < 0)
         {
            Serial2.println("CAL,NUTRIENT_PUMP,ERROR,INVALID_PUMP");
            return;
         }
         bool ok = nutrientPumps.runPump(pumpId, durationMs, pumpSpeed);
         if (ok)
         {
            Serial2.print("CAL,NUTRIENT_PUMP,COMPLETE,");
            Serial2.println(nutrientPumpName);
         }
         else
         {
            Serial2.print("CAL,NUTRIENT_PUMP,ERROR,");
            Serial2.println(nutrientPumpName);
         }
      }
      else if (cmd.startsWith("AUX_RESERVOIR_CONFIG"))
      {
         String payload = cmd.substring(21); // drop "AUX_RESERVOIR_CONFIG,"
         int i1 = payload.indexOf(',');
         int i2 = payload.indexOf(',', i1 + 1);
         auxReservoirMinWeight = payload.substring(i1 + 1, i2).toInt();
         auxReservoirMaxWeight = payload.substring(i2 + 1).toInt();
      }
   }
}

void sendStatus()
{
   int mainInletValveOpen = mainReservoirInletValve.isValveOpen() ? 1 : 0;
   int dutchBucketInletValveOpen = dutchBucketInletValve.isValveOpen() ? 1 : 0;
   int ebbFlowInletValveOpen = ebbFlowInletValve.isValveOpen() ? 1 : 0;

   // MR = Main Reservoir, IV = Inlet Valve, DB = Dutch Bucket, EF = Ebb & Flow, O = Open

   Serial2.print("STAT,MR_Status=");
   Serial2.print(mainReservoirInletValve.getReservoirStatus());
   Serial2.print(",MR_IV_O=");
   Serial2.print(mainInletValveOpen);
   Serial2.print(",DB_Weight=");
   Serial2.print(lastDutchBucketWeight);
   Serial2.print(",DB_IV_O=");
   Serial2.print(dutchBucketInletValveOpen);
   Serial2.print(",EF_Weight=");
   Serial2.print(lastEbbFlowWeight);
   Serial2.print(",EF_IV_O=");
   Serial2.print(ebbFlowInletValveOpen);
   Serial2.print(",MR_IV_Flow=");
   Serial2.println(mainInletValveflowRate, 2);
}