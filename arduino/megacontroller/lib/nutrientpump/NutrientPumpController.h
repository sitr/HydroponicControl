#ifndef NUTRIENT_PUMP_H
#define NUTRIENT_PUMP_H

#include <PwmControl.h>

enum NutrientPumpId : int
{
   FLORA_GROW_PUMP = 0,
   FLORA_MICRO_PUMP = 1,
   FLORA_BLOOM_PUMP = 2,
   PH_DOWN_PUMP = 3,
   NUTRIENT_PUMP_COUNT = 4
};

class NutrientPumpController
{
public:
   NutrientPumpController(
      PwmControl &floraGrowPump,
      PwmControl &floraMicroPump,
      PwmControl &floraBloomPump,
      PwmControl &phDownPump,
      Print &debugOut
   );

   void begin();

   bool runPump(int pumpId, unsigned long durationMs, int speed);
   bool doseMl(int pumpId, float amountMl, int speed);

   bool setFlowRate(int pumpId, float flowMlPerSec);
   float getFlowRate(int pumpId) const;

   const char *pumpIdToName(int pumpId) const;

private:
   struct CalibrationConfig
   {
      uint32_t magic;
      uint16_t version;
      float flowMlPerSec[NUTRIENT_PUMP_COUNT];
      uint16_t checksum;
   };

   PwmControl *_pumps[NUTRIENT_PUMP_COUNT];
   Print &_debugOut;

   float _flowMlPerSec[NUTRIENT_PUMP_COUNT];

   static constexpr uint32_t EEPROM_MAGIC = 0x4E504341; // "NPCA"
   static constexpr uint16_t EEPROM_VERSION = 1;

   // Change this if other classes use EEPROM.
   static constexpr int EEPROM_ADDR = 100;
   static constexpr unsigned long MAX_PUMP_RUN_MS = 60000UL;
   static constexpr float DEFAULT_GROW_FLOW_ML_PER_SEC = 0.8f;
   static constexpr float DEFAULT_MICRO_FLOW_ML_PER_SEC = 0.75f;
   static constexpr float DEFAULT_BLOOM_FLOW_ML_PER_SEC = 0.85f;
   static constexpr float DEFAULT_PH_DOWN_FLOW_ML_PER_SEC = 0.7f;

   uint16_t calculateChecksum(const CalibrationConfig &config) const;

   void loadCalibration();
   void saveCalibration();

   unsigned long pumpMsForMl(float ml, float mlPerSec, int speed) const;
   bool isValidPumpId(int pumpId) const;
   };
#endif