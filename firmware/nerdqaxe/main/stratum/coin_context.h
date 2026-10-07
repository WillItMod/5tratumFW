#pragma once
#include <cstring>

// Explicit configured route identity, never inferred from a hostname, wallet,
// algorithm or Bitcoin-formatted Stratum fields. A peer declaration only.
struct MuxCoinContext {
    bool available = false;
    char id[33] = {};
    char ticker[13] = {};
    char name[49] = {};
};

inline bool isBitcoinCoinContext(const MuxCoinContext &coin) {
    return coin.available && !strcmp(coin.id, "bitcoin") && !strcmp(coin.ticker, "BTC");
}
