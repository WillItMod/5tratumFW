#pragma once

#include <cstdint>
#include "ArduinoJson.h"

namespace Config {

// ArduinoJson's as<uint16_t>() coerces booleans, strings and floats. Capability
// discovery must describe actual integer settings, never a converted default.
inline bool readStrictU16(JsonVariantConst value, uint16_t &output) {
    if (!value.is<uint16_t>()) return false;
    output = value.as<uint16_t>();
    return true;
}

} // namespace Config
