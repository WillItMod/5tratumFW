#ifndef FIVE_TRATUM_MUX_STATUS_H
#define FIVE_TRATUM_MUX_STATUS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "cJSON.h"

#define FIVE_TRATUM_MUX_METHOD "mining.5tratum.status"
#define FIVE_TRATUM_MUX_TTL_SECONDS 90

typedef struct {
    uint64_t generation;
    bool connected;
    bool advertised;
    bool fallback;
    int64_t received_us;
} FiveTratumMuxPeer;

typedef struct {
    bool connected;
    bool acknowledged;
    bool expired;
    bool fallback;
    uint32_t age_seconds;
} FiveTratumMuxSnapshot;

// Caller serializes these bounded state copies. Peer status is an advertisement,
// not cryptographic authentication or evidence of independent ASIC targeting.
static inline uint64_t five_tratum_mux_begin(FiveTratumMuxPeer *peer, bool fallback)
{
    if (++peer->generation == 0) ++peer->generation;
    peer->connected = true;
    peer->advertised = false;
    peer->fallback = fallback;
    peer->received_us = 0;
    return peer->generation;
}

static inline void five_tratum_mux_disconnect(FiveTratumMuxPeer *peer)
{
    peer->connected = false;
    peer->advertised = false;
    peer->received_us = 0;
}

static inline void five_tratum_mux_receive(FiveTratumMuxPeer *peer, uint64_t generation,
                                           bool valid, int64_t now_us)
{
    if (!peer->connected || peer->generation != generation) return;
    peer->advertised = valid && now_us >= 0;
    peer->received_us = peer->advertised ? now_us : 0;
}

static inline FiveTratumMuxSnapshot five_tratum_mux_snapshot(const FiveTratumMuxPeer *peer,
                                                            int64_t now_us)
{
    FiveTratumMuxSnapshot result = {.connected = peer->connected, .fallback = peer->fallback};
    if (!peer->connected || !peer->advertised) return result;
    if (now_us < peer->received_us) {
        result.expired = true;
        return result;
    }
    uint64_t age = (uint64_t)(now_us - peer->received_us);
    uint64_t seconds = age / 1000000ULL;
    result.age_seconds = seconds > UINT32_MAX ? UINT32_MAX : (uint32_t)seconds;
    result.acknowledged = age < FIVE_TRATUM_MUX_TTL_SECONDS * 1000000ULL;
    result.expired = !result.acknowledged;
    return result;
}

// Consume even malformed reserved messages before normal result/share parsing.
static inline bool five_tratum_mux_reserved(const cJSON *json)
{
    if (!cJSON_IsObject(json)) return false;
    for (const cJSON *item = json->child; item; item = item->next) {
        if (item->string && !strcmp(item->string, "method") && cJSON_IsString(item) &&
            !strcmp(item->valuestring, FIVE_TRATUM_MUX_METHOD)) return true;
    }
    return false;
}

static inline bool five_tratum_mux_json_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// cJSON stores every JSON number as a double, losing its integer token type.
// This bounded advertisement has exactly two numbers; inspect their tokens
// outside strings so floats, exponents and leading zeros cannot pass by rounding.
static inline bool five_tratum_mux_integer_tokens(const char *raw)
{
    unsigned numbers = 0;
    const char *p = raw;
    while (*p) {
        if (*p == '"') {
            ++p;
            while (*p && *p != '"') {
                if ((unsigned char)*p < 0x20) return false;
                if (*p == '\\') {
                    ++p;
                    if (!*p) return false;
                }
                ++p;
            }
            if (!*p) return false;
            ++p;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            if (*p == '-') ++p;
            if (*p < '0' || *p > '9') return false;
            if (*p == '0') {
                ++p;
                if (*p >= '0' && *p <= '9') return false;
            } else {
                do { ++p; } while (*p >= '0' && *p <= '9');
            }
            if (*p && !five_tratum_mux_json_space(*p) && *p != ',' && *p != ']' && *p != '}') return false;
            ++numbers;
        } else {
            if ((unsigned char)*p < 0x20 && !five_tratum_mux_json_space(*p)) return false;
            ++p;
        }
    }
    return numbers == 2;
}

static inline bool five_tratum_mux_status_valid(const cJSON *json, const char *raw, const char *parse_end)
{
    // cJSON does not retain lengths after an embedded NUL. Reject its JSON escape
    // before comparing decoded protocol strings. No extra allocation is required.
    if (!raw || !parse_end || strlen(raw) > 512 || strstr(raw, "\\u0000") || !cJSON_IsObject(json)) return false;
    // Keep the parsed envelope available for reserved-method interception, but
    // require a complete single JSON value before acknowledging the peer.
    for (const char *p = parse_end; *p; ++p) {
        if (!five_tratum_mux_json_space(*p)) return false;
    }
    if (!five_tratum_mux_integer_tokens(raw)) return false;
    unsigned envelope = 0;
    for (const cJSON *item = json->child; item; item = item->next) {
        unsigned bit = 0;
        if (!strcmp(item->string, "id") && cJSON_IsNull(item)) bit = 1;
        else if (!strcmp(item->string, "method") && cJSON_IsString(item) &&
                 !strcmp(item->valuestring, FIVE_TRATUM_MUX_METHOD)) bit = 2;
        else if (!strcmp(item->string, "params") && cJSON_IsArray(item)) bit = 4;
        else if (!strcmp(item->string, "jsonrpc") && cJSON_IsString(item) &&
                 !strcmp(item->valuestring, "2.0")) bit = 8;
        if (!bit || (envelope & bit)) return false;
        envelope |= bit;
    }
    if ((envelope & 7) != 7) return false;
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(json, "params");
    if (cJSON_GetArraySize(params) != 1) return false;
    const cJSON *status = cJSON_GetArrayItem(params, 0);
    if (!cJSON_IsObject(status)) return false;
    unsigned fields = 0;
    for (const cJSON *item = status->child; item; item = item->next) {
        unsigned bit = 0;
        if (!strcmp(item->string, "protocolVersion") && cJSON_IsNumber(item) && item->valuedouble == 1) bit = 1;
        else if (!strcmp(item->string, "product") && cJSON_IsString(item) && !strcmp(item->valuestring, "5tratMUX")) bit = 2;
        else if (!strcmp(item->string, "workScope") && cJSON_IsString(item) && !strcmp(item->valuestring, "chain-broadcast")) bit = 4;
        else if (!strcmp(item->string, "independentWorkAssignment") && cJSON_IsFalse(item)) bit = 8;
        else if (!strcmp(item->string, "ttlSeconds") && cJSON_IsNumber(item) && item->valuedouble == FIVE_TRATUM_MUX_TTL_SECONDS) bit = 16;
        if (!bit || (fields & bit)) return false;
        fields |= bit;
    }
    return fields == 31;
}

#endif
