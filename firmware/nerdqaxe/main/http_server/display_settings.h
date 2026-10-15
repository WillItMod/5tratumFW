#pragma once

#include <cstdlib>
#include "nvs_config.h"
#include "displays/display_profile_json.h"

namespace FiveTratumDisplay {
inline void appendSavedDisplayProfile(JsonDocument &doc) {
    char *saved = Config::getDisplayProfile();
    doc["displayProfile"] = savedDisplayProfile(saved).id;
    std::free(saved);
}
inline void appendDisplayRuntimeMetadata(JsonDocument &doc) {
    char *saved = Config::getDisplayProfile();
    appendDisplayRuntimeMetadata(doc.as<JsonObject>(), saved, displayProfileRuntime.active());
    std::free(saved);
}
} // namespace FiveTratumDisplay
