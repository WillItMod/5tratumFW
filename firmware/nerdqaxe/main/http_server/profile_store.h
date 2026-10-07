#pragma once
#include "profile_schema.h"
namespace FiveTratumProfiles {
enum class StoreResult { Ok, Empty, Failure };
StoreResult readSlot(Kind kind, unsigned slot, JsonDocument &record);
StoreResult saveSlot(Kind kind, unsigned slot, JsonObjectConst record);
StoreResult clearSlot(Kind kind, unsigned slot);
}
