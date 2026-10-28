#pragma once
#include "profile_schema.h"
namespace FiveTratumProfiles {
enum class StoreResult { Ok, Empty, Failure };
// One recursive transaction lock protects profile records and schedule references.
// Callers may hold it across validation + commit, including the slot-delete guard.
class ProfileStoreGuard {
public:
    ProfileStoreGuard();
    ~ProfileStoreGuard();
    ProfileStoreGuard(const ProfileStoreGuard &) = delete;
    ProfileStoreGuard &operator=(const ProfileStoreGuard &) = delete;
};
StoreResult readSlot(Kind kind, unsigned slot, JsonDocument &record);
StoreResult saveSlot(Kind kind, unsigned slot, JsonObjectConst record);
StoreResult clearSlot(Kind kind, unsigned slot);
}
