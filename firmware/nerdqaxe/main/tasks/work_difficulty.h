#pragma once
// Read-only metadata for dispatched work. No pool control or external data.
#include <cmath>
#include <cstdint>

namespace FiveTratumWork {
struct Stamp { uint64_t epoch=0, receivedAtUs=0; };
struct Sample { bool available=false; double shareDifficulty=0, networkDifficulty=0; uint64_t receivedAtUs=0; };
struct Pool { bool selected=false, connected=false; Sample work{}; };
struct Snapshot { Pool pools[2]{}; uint64_t nowUs=0; };

// Exact compact-target value, normalized to the SHA256 Bitcoin diff-1 target.
// This identifies units, never a coin or an externally fetched network value.
inline bool targetDifficulty(uint32_t bits,double &out) {
    out=0;
    const unsigned size=bits>>24;
    uint32_t word=bits&0x007fffffU;
    if(!word || (bits&0x00800000U) || size>34 || (word>0xffU&&size>33) || (word>0xffffU&&size>32)) return false;
    unsigned exponent=size;
    if(size<=3) {word>>=8*(3-size);exponent=3;}
    if(!word) return false;
    const double value=std::ldexp(65535.0/static_cast<double>(word),8*(29-static_cast<int>(exponent)));
    if(!std::isfinite(value)||value<=0) return false;
    out=value;return true;
}
}
