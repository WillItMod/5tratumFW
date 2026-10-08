#pragma once
#include <cstring>

namespace FiveTratumMining {
// Exact standalone driver identities. A chip count alone must never enable
// reversible power control for a different board or ASIC family.
inline unsigned expectedStandaloneChain(const char *model, const char *asic) {
    if (!model || !asic) return 0;
    if (!std::strcmp(model, "NerdQAxe+") && !std::strcmp(asic, "BM1368")) return 4;
    if (!std::strcmp(model, "NerdQAxe++") && !std::strcmp(asic, "BM1370")) return 4;
    if (!std::strcmp(model, "NerdOCTAXE-γ") && !std::strcmp(asic, "BM1370")) return 8;
    return 0;
}
}
