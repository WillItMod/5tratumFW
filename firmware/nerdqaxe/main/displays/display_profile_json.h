#pragma once

#include "ArduinoJson.h"
#include "display_profile.h"

namespace FiveTratumDisplay {

struct DisplayProfilePatch {
    const char *id = nullptr; // canonical static identifier; null means omitted
    const char *error = nullptr;
};

inline DisplayProfilePatch validateDisplayProfilePatch(
    JsonObjectConst doc, bool wide = SupportsRuntimeWideProfiles) {
    JsonVariantConst value = doc["displayProfile"];
    if (value.isUnbound()) return {};
    if (!value.is<const char *>()) return {nullptr, "displayProfile must be a supported string ID"};
    const JsonString text = value.as<JsonString>();
    // A JSON string can contain embedded NUL; strcmp alone would accept a
    // valid prefix and silently discard an unknown suffix.
    if (!text.c_str() || text.size() != std::strlen(text.c_str()))
        return {nullptr, "Invalid displayProfile"};
    const auto *profile = findDisplayProfile(text.c_str());
    if (!displayProfileSupported(profile, wide))
        return {nullptr, "displayProfile is not supported by this miner"};
    return {profile->id, nullptr};
}

inline void appendDisplayRuntimeMetadata(JsonObject doc, const char *rawSaved,
    const DisplayProfile &active, bool wide = SupportsRuntimeWideProfiles) {
    const auto &saved = savedDisplayProfile(rawSaved, wide);
    doc["displayProfile"] = saved.id;
    doc["displayActiveProfile"] = active.id;
    doc["displayProfileCapabilityVersion"] = 1;
    doc["displayProfileStorageValid"] = displayProfileSupported(findDisplayProfile(rawSaved), wide);
    JsonArray options = doc["displayProfileOptions"].to<JsonArray>();
    options.add(StandardPanelProfile.id);
    if (wide) { options.add(WideRgbPanelProfile.id); options.add(WideBgrPanelProfile.id); }
    doc["displayRestartRequired"] = displayRestartRequired(rawSaved, active, wide);
    doc["displayWidth"] = active.width;
    doc["displayHeight"] = active.height;
    doc["displayProfileExperimental"] = active.experimental;
}

} // namespace FiveTratumDisplay
