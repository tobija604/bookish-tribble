#pragma once
#include <Arduino.h>
#include "../users/User.h" // reuse AccessSchedule

struct RfidCard {
  String   uid;              // hex string, e.g. "04A1B2C3"
  String   userId;           // links to SmartBoxUser::id
  String   label;            // optional friendly name, e.g. "Miha's badge"
  bool     enabled = true;
  uint32_t createdEpoch = 0;
  uint32_t lastUsedEpoch = 0;
  uint32_t useCount = 0;
  AccessSchedule schedule;   // card-level override; if disabled, falls back to the user's own schedule
};
