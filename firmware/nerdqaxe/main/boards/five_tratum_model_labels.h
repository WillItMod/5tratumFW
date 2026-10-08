#pragma once

#include <cstring>

namespace FiveTratumModels {

// The public model keeps its existing UTF-8 name. Subscribe components have a
// separate ASCII name so the bounded, JSON-safe agent builder stays strict.
constexpr char OctaxeGamma[] = "NerdOCTAXE-\xCE\xB3";
constexpr char OctaxeGammaMiningAgent[] = "NerdOCTAXE-Gamma";

inline bool isOctaxeGamma(const char *value) noexcept {
    return value && std::strncmp(value, OctaxeGamma, sizeof(OctaxeGamma)) == 0;
}

} // namespace FiveTratumModels
