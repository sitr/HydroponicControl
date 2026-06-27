#include <Arduino.h>
#include <EEPROM.h>
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

static constexpr uint32_t CONFIG_EEPROM_MAGIC = 0x43464743; // "CFGC"
static constexpr uint16_t CONFIG_EEPROM_VERSION = 1;
static constexpr int CONFIG_EEPROM_ADDR = 200;

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
// Dosing configuration (set via CONFIG command: min,max,flora_grow,flora_bloom,flora_micro,ph)
int floraGrowDosing = 0;
int floraBloomDosing = 0;
int floraMicroDosing = 0;
float phDosing = 0.0f;

struct ConfigData {
   uint32_t magic;
   uint16_t version;
   float minWeight;
   float maxWeight;
   int32_t floraGrowDosing;
   int32_t floraBloomDosing;
   int32_t floraMicroDosing;
   float phDosing;
   uint16_t checksum;
};

uint16_t calculateConfigChecksum(const ConfigData &config);
void loadControlConfig();
void saveControlConfig();

PwmControl floraGrowPump(FLORA_GROW_PUMP_PIN, false);
PwmControl floraMicroPump(FLORA_MICRO_PUMP_PIN, false);
PwmControl floraBloomPump(FLORA_BLOOM_PUMP_PIN, false);
PwmControl phDownPump(PH_DOWN_PUMP_PIN, false);

NutrientPumpController nutrientPumps(floraGrowPump, floraMicroPump, floraBloomPump, phDownPump, Serial);

ReservoirInletValve mainReservoirInletValve(RESERVOIR_INLET_VALVE_PIN, RESERVOIR_TOP_SENSOR_PIN, RESERVOIR_BOTTOM_SENSOR_PIN);
ReservoirInletValve dutchBucketInletValve(DB_INLET_VALVE_PIN);
ReservoirInletValve ebbFlowInletValve(EF_INLET_VALVE_PIN);
ReservoirInletValve dutchBucketNutrientValve(DB_NUTRIENT_VALVE_PIN);
ReservoirInletValve ebbFlowNutrientValve(EF_NUTRIENT_VALVE_PIN);
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

   loadControlConfig();

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
         Serial2.println("DB_VALVE,OPEN");
      }
      else if (cmd == "CLOSE_DB_VALVE")
      {
         dutchBucketInletValve.closeValve();
         Serial2.println("DB_VALVE,CLOSED");
      }
      else if (cmd == "OPEN_EF_VALVE")
      {
         ebbFlowInletValve.openValve();
         Serial2.println("EF_VALVE,OPEN");
      }
      else if (cmd == "CLOSE_EF_VALVE")
      {
         ebbFlowInletValve.closeValve();
         Serial2.println("EF_VALVE,CLOSED");
      }
      else if (cmd == "START_DB_SCALECAL")
      {
         dutchBucketScale.beginCalMode();
      }
      else if (cmd.startsWith("CAL_FACTOR_DB_SCALE"))
      {
         String payload = cmd.substring(20);
         Serial.println("Setting DB scale calibration factor to: " + payload);
         long calibrationFactor = payload.toInt();
         dutchBucketScale.setCalibrationFactor(calibrationFactor);
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
      else if (cmd.startsWith("CONFIG"))
      {
         String payload = cmd.substring(7); // drop "CONFIG,"
         Serial.println("Received hydroponic controller config: " + payload);
         // Expecting: minWeight,maxWeight,flora_grow_dosing,flora_bloom_dosing,flora_micro_dosing,ph_dosing
         int i1 = payload.indexOf(',');
         int i2 = payload.indexOf(',', i1 + 1);
         int i3 = payload.indexOf(',', i2 + 1);
         int i4 = payload.indexOf(',', i3 + 1);
         int i5 = payload.indexOf(',', i4 + 1);

         if (!(i1 > 0 && i2 > i1 && i3 > i2 && i4 > i3 && i5 > i4)) {
            Serial.println("CONFIG: invalid payload, expected 6 comma-separated values");
         }
         else {
            auxReservoirMinWeight = payload.substring(0, i1).toFloat();
            auxReservoirMaxWeight = payload.substring(i1 + 1, i2).toFloat();
            floraGrowDosing = payload.substring(i2 + 1, i3).toInt();
            floraBloomDosing = payload.substring(i3 + 1, i4).toInt();
            floraMicroDosing = payload.substring(i4 + 1, i5).toInt();
            phDosing = payload.substring(i5 + 1).toFloat();
            saveControlConfig();
            Serial.print("Config set: min="); Serial.print(auxReservoirMinWeight);
            Serial.print(", max="); Serial.print(auxReservoirMaxWeight);
            Serial.print(", flora_grow="); Serial.print(floraGrowDosing);
            Serial.print(", flora_bloom="); Serial.print(floraBloomDosing);
            Serial.print(", flora_micro="); Serial.print(floraMicroDosing);
            Serial.print(", ph="); Serial.println(phDosing);
         }
      }
   }
}

uint16_t calculateConfigChecksum(const ConfigData &config)
{
   const uint8_t *data = reinterpret_cast<const uint8_t *>(&config);
   uint16_t sum = 0;

   for (size_t i = 0; i < sizeof(ConfigData) - sizeof(config.checksum); i++)
   {
      sum += data[i];
   }

   return sum;
}

void loadControlConfig()
{
   ConfigData config;
   EEPROM.get(CONFIG_EEPROM_ADDR, config);

   bool valid =
      config.magic == CONFIG_EEPROM_MAGIC &&
      config.version == CONFIG_EEPROM_VERSION &&
      config.checksum == calculateConfigChecksum(config);

   if (!valid)
   {
      Serial.println("No valid hydroponic config in EEPROM. Using default values.");
      return;
   }

   auxReservoirMinWeight = config.minWeight;
   auxReservoirMaxWeight = config.maxWeight;
   floraGrowDosing = static_cast<int>(config.floraGrowDosing);
   floraBloomDosing = static_cast<int>(config.floraBloomDosing);
   floraMicroDosing = static_cast<int>(config.floraMicroDosing);
   phDosing = config.phDosing;

   Serial.print("Loaded hydroponic config from EEPROM: min="); Serial.print(auxReservoirMinWeight);
   Serial.print(", max="); Serial.print(auxReservoirMaxWeight);
   Serial.print(", flora_grow="); Serial.print(floraGrowDosing);
   Serial.print(", flora_bloom="); Serial.print(floraBloomDosing);
   Serial.print(", flora_micro="); Serial.print(floraMicroDosing);
   Serial.print(", ph="); Serial.println(phDosing);
}

void saveControlConfig()
{
   ConfigData config;
   memset(&config, 0, sizeof(config));
   config.magic = CONFIG_EEPROM_MAGIC;
   config.version = CONFIG_EEPROM_VERSION;
   config.minWeight = auxReservoirMinWeight;
   config.maxWeight = auxReservoirMaxWeight;
   config.floraGrowDosing = floraGrowDosing;
   config.floraBloomDosing = floraBloomDosing;
   config.floraMicroDosing = floraMicroDosing;
   config.phDosing = phDosing;
   config.checksum = calculateConfigChecksum(config);

   EEPROM.put(CONFIG_EEPROM_ADDR, config);
   Serial.println("Saved hydroponic config to EEPROM.");
}

void sendStatus()
{
   // MR = Main Reservoir, IV = Inlet Valve, DB = Dutch Bucket, EF = Ebb & Flow, O = Open
   // NV = Nutrient Valve

   Serial2.print("STAT,MR_Status=");
   Serial2.print(mainReservoirInletValve.getReservoirStatus());
   Serial2.print(",MR_IV_O=");
   Serial2.print(mainReservoirInletValve.isValveOpen() ? 1 : 0);
   Serial2.print(",DB_Weight=");
   Serial2.print(lastDutchBucketWeight);
   Serial2.print(",DB_IV_O=");
   Serial2.print(dutchBucketInletValve.isValveOpen() ? 1 : 0);
   Serial2.print(",EF_Weight=");
   Serial2.print(lastEbbFlowWeight);
   Serial2.print(",EF_IV_O=");
   Serial2.print(ebbFlowInletValve.isValveOpen() ? 1 : 0);
   Serial2.print(",MR_IV_Flow=");
   Serial2.print(mainInletValveflowRate, 2);
   Serial2.print(",EF_NV_O=");
   Serial2.print(ebbFlowNutrientValve.isValveOpen() ? 1 : 0);
   Serial2.print(",DB_NV_O=");
   Serial2.println(dutchBucketNutrientValve.isValveOpen() ? 1 : 0);
}