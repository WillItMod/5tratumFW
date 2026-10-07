#pragma once
#include <ArduinoJson.h>
namespace FiveTratumProfiles {
// Commit first, then publish by a nonallocating swap. The caller holds the config mutex.
inline bool applyPersistedPatch(JsonDocument &current, JsonObjectConst patch,
                                bool (*commit)(const JsonDocument &, void *), void *context) {
    if (patch.isNull() || patch.size()==0 || patch.size()>12) return false;
    JsonDocument next(current.allocator());
    if (!next.set(current)) return false;
    for (JsonPairConst pair:patch) next[pair.key()]=pair.value();
    if (next.overflowed() || !commit(next,context)) return false;
    swap(current,next);
    return true;
}
}
