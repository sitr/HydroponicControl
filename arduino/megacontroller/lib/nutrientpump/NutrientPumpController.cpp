#include "NutrientPumpController.h"
#include <EEPROM.h>
#include <string.h>

NutrientPumpController::NutrientPumpController(
   PwmControl &floraGrowPump,
   PwmControl &floraMicroPump,
   PwmControl &floraBloomPump,
   PwmControl &phDownPump,
   Print &debugOut
)
   : _pumps{&floraGrowPump, &floraMicroPump, &floraBloomPump, &phDownPump},
     _debugOut(debugOut),
     _flowMlPerSec{
        DEFAULT_GROW_FLOW_ML_PER_SEC,
        DEFAULT_MICRO_FLOW_ML_PER_SEC,
        DEFAULT_BLOOM_FLOW_ML_PER_SEC,
        DEFAULT_PH_DOWN_FLOW_ML_PER_SEC
     }
{
}

void NutrientPumpController::begin()
{
   loadCalibration();
}

bool NutrientPumpController::runPump(int pumpId, unsigned long durationMs, int speed)
{
   if (!isValidPumpId(pumpId))
   {
      _debugOut.println("Nutrient pump error: invalid pump ID");
      return false;
   }

   if (durationMs == 0)
   {
      _debugOut.println("Nutrient pump error: invalid duration");
      return false;
   }

   if (durationMs > MAX_PUMP_RUN_MS)
   {
      _debugOut.println("Nutrient pump error: duration too long");
      return false;
   }

   if (speed <= 0)
   {
      _debugOut.println("Nutrient pump error: invalid speed");
      return false;
   }

   speed = constrain(speed, 0, 100);

   PwmControl *pump = _pumps[pumpId];

   if (!pump)
   {
      _debugOut.println("Nutrient pump error: null pump");
      return false;
   }

   _debugOut.print("Running nutrient pump ");
   _debugOut.print(pumpIdToName(pumpId));
   _debugOut.print(" for ");
   _debugOut.print(durationMs);
   _debugOut.print(" ms at speed ");
   _debugOut.println(speed);

   pump->setSpeed(speed);
   delay(durationMs);
   pump->off();

   return true;
}

bool NutrientPumpController::doseMl(int pumpId, float amountMl, int speed)
{
   if (!isValidPumpId(pumpId))
   {
      _debugOut.println("Nutrient dose error: invalid pump ID");
      return false;
   }

   if (amountMl <= 0.0f)
   {
      _debugOut.println("Nutrient dose error: invalid amount");
      return false;
   }

   if (speed <= 0)
   {
      _debugOut.println("Nutrient dose error: invalid speed");
      return false;
   }

   float flowRate = getFlowRate(pumpId);

   if (flowRate <= 0.0f)
   {
      _debugOut.println("Nutrient dose error: invalid flow rate");
      return false;
   }

   unsigned long durationMs = pumpMsForMl(amountMl, flowRate, speed);

   if (durationMs == 0)
   {
      _debugOut.println("Nutrient dose error: calculated duration is zero");
      return false;
   }

   _debugOut.print("Dosing ");
   _debugOut.print(amountMl);
   _debugOut.print(" ml from ");
   _debugOut.print(pumpIdToName(pumpId));
   _debugOut.print(" using flow ");
   _debugOut.print(flowRate, 4);
   _debugOut.print(" ml/sec at speed ");
   _debugOut.print(speed);
   _debugOut.print(" for ");
   _debugOut.print(durationMs);
   _debugOut.println(" ms");

   return runPump(pumpId, durationMs, speed);
}

bool NutrientPumpController::setFlowRate(int pumpId, float flowMlPerSec)
{
   if (!isValidPumpId(pumpId))
      return false;

   if (flowMlPerSec <= 0.0f || flowMlPerSec > 100.0f)
      return false;

   _flowMlPerSec[pumpId] = flowMlPerSec;
   saveCalibration();

   return true;
}

float NutrientPumpController::getFlowRate(int pumpId) const
{
   if (!isValidPumpId(pumpId))
      return 0.0f;

   return _flowMlPerSec[pumpId];
}

const char *NutrientPumpController::pumpIdToName(int pumpId) const
{
   switch (pumpId)
   {
      case FLORA_GROW_PUMP:
         return "FLORA_GROW";

      case FLORA_MICRO_PUMP:
         return "FLORA_MICRO";

      case FLORA_BLOOM_PUMP:
         return "FLORA_BLOOM";

      default:
         return "UNKNOWN";
   }
}

bool NutrientPumpController::isValidPumpId(int pumpId) const
{
   return pumpId >= 0 && pumpId < NUTRIENT_PUMP_COUNT;
}

unsigned long NutrientPumpController::pumpMsForMl(float ml, float mlPerSec, int speed) const
{
   if (ml <= 0.0f || mlPerSec <= 0.0f || speed <= 0)
      return 0;

   float effectiveFlow = mlPerSec * (speed / 100.0f);

   if (effectiveFlow <= 0.0f)
      return 0;

   return (unsigned long)((ml / effectiveFlow) * 1000.0f);
}

uint16_t NutrientPumpController::calculateChecksum(const CalibrationConfig &config) const
{
   const uint8_t *data = reinterpret_cast<const uint8_t *>(&config);
   uint16_t sum = 0;

   for (size_t i = 0; i < sizeof(CalibrationConfig) - sizeof(config.checksum); i++)
   {
      sum += data[i];
   }

   return sum;
}

void NutrientPumpController::loadCalibration()
{
   CalibrationConfig config;
   EEPROM.get(EEPROM_ADDR, config);

   bool valid =
      config.magic == EEPROM_MAGIC &&
      config.version == EEPROM_VERSION &&
      config.checksum == calculateChecksum(config);

   if (!valid)
   {
      _debugOut.println("No valid nutrient pump calibration in EEPROM. Using defaults.");
      saveCalibration();
      return;
   }

   for (int i = 0; i < NUTRIENT_PUMP_COUNT; i++)
   {
      if (config.flowMlPerSec[i] > 0.0f && config.flowMlPerSec[i] <= 100.0f)
      {
         _flowMlPerSec[i] = config.flowMlPerSec[i];
      }
   }

   _debugOut.println("Loaded nutrient pump calibration from EEPROM.");
}

void NutrientPumpController::saveCalibration()
{
   CalibrationConfig config;
   memset(&config, 0, sizeof(config));

   config.magic = EEPROM_MAGIC;
   config.version = EEPROM_VERSION;

   for (int i = 0; i < NUTRIENT_PUMP_COUNT; i++)
   {
      config.flowMlPerSec[i] = _flowMlPerSec[i];
   }

   config.checksum = calculateChecksum(config);

   EEPROM.put(EEPROM_ADDR, config);

   _debugOut.println("Saved nutrient pump calibration to EEPROM.");
}