#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

namespace FiveTratumProfiles {
constexpr unsigned SLOT_COUNT = 10;
constexpr size_t MAX_REQUEST_BYTES = 8192;
constexpr size_t MAX_SLOT_BYTES = 1536;
struct Limits { uint16_t minFrequency = 50, maxFrequency = 800, minVoltage = 1005, maxVoltage = 1400; };
enum class Kind { Tuning, Pool };
struct Request { Kind kind; unsigned slot; bool clear = false; bool directTuning = false; int capturePool = -1; int targetPool = -1; };
// Pure validation/normalization: used by actual HTTP handlers and sanitizer tests.
bool parseRequest(JsonObjectConst input, bool apply, Request &request);
bool buildRecord(JsonObjectConst input, const Request &request, JsonObjectConst previous,
                 JsonObjectConst captured, const Limits &limits, JsonDocument &output);
bool validRecord(JsonObjectConst record, Kind kind);
bool validTuning(JsonObjectConst record, const Limits &limits);
void writePublicSlot(JsonObject output, unsigned slot, Kind kind, JsonObjectConst record);
void buildActivePatch(JsonObject patch, Kind kind, JsonObjectConst record, int pool);
}
