#include "five_tratum_status_report.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool exact_fields(const cJSON *object, const char *const *names, size_t count)
{
    if (!cJSON_IsObject(object)) return false;
    size_t seen = 0;
    for (const cJSON *item = object->child; item; item = item->next) {
        if (!item->string) return false;
        size_t index = 0;
        while (index < count && strcmp(item->string, names[index])) ++index;
        if (index == count || (seen & ((size_t)1 << index)) != 0) return false;
        seen |= (size_t)1 << index;
    }
    return seen == (((size_t)1 << count) - 1);
}

static bool text_equals(const cJSON *object, const char *name, const char *expected)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(value) && value->valuestring && !strcmp(value->valuestring, expected);
}

static bool valid_binding(const cJSON *binding)
{
    static const char *const root_fields[] = {"identity", "hardware", "firmware"};
    static const char *const identity_fields[] = {"deviceId"};
    static const char *const hardware_fields[] = {"boardModel", "asicModel", "asicCount"};
    static const char *const firmware_fields[] = {"product", "version"};
    if (!exact_fields(binding, root_fields, 3)) return false;
    const cJSON *identity = cJSON_GetObjectItemCaseSensitive(binding, "identity");
    const cJSON *hardware = cJSON_GetObjectItemCaseSensitive(binding, "hardware");
    const cJSON *firmware = cJSON_GetObjectItemCaseSensitive(binding, "firmware");
    if (!exact_fields(identity, identity_fields, 1) ||
        !exact_fields(hardware, hardware_fields, 3) ||
        !exact_fields(firmware, firmware_fields, 2)) return false;

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(identity, "deviceId");
    if (!cJSON_IsString(id) || !id->valuestring ||
        strnlen(id->valuestring, 38) != 37 || strncmp(id->valuestring, "5tfw:", 5)) return false;
    bool nonzero = false;
    for (size_t i = 5; i < 37; ++i) {
        const char ch = id->valuestring[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
        nonzero |= ch != '0';
    }
    if (!nonzero || !text_equals(firmware, "product", "5tratumFW") ||
        !text_equals(hardware, "asicModel", "BM1370") ||
        !(text_equals(hardware, "boardModel", "Gamma 601") ||
          text_equals(hardware, "boardModel", "Gamma 602"))) return false;
    const cJSON *count = cJSON_GetObjectItemCaseSensitive(hardware, "asicCount");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(firmware, "version");
    if (!cJSON_IsNumber(count) || count->valuedouble != 1 ||
        !cJSON_IsString(version) || !version->valuestring) return false;
    const size_t length = strnlen(version->valuestring, 33);
    if (length < 11 || length > 32 || strncmp(version->valuestring, "5tratumFW-", 10)) return false;
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char)version->valuestring[i] < 32 ||
            (unsigned char)version->valuestring[i] > 126) return false;
    return true;
}

cJSON *five_tratum_status_report(const cJSON *binding, const FiveTratumStatusReport *snapshot)
{
    if (!snapshot || !valid_binding(binding) || snapshot->uptime_seconds > 9007199254740991ULL) return NULL;
    cJSON *root = cJSON_Duplicate(binding, true);
    if (!root) return NULL;
    cJSON *work = cJSON_AddObjectToObject(root, "work");
    cJSON *mux = cJSON_AddObjectToObject(root, "mux");
    cJSON *pools = mux ? cJSON_AddArrayToObject(mux, "pools") : NULL;
    if (!work || !mux || !pools ||
        !cJSON_AddNumberToObject(root, "schemaVersion", 1) ||
        !cJSON_AddNumberToObject(root, "observedUptimeSeconds", (double)snapshot->uptime_seconds) ||
        !cJSON_AddStringToObject(work, "scope", "chain-broadcast") ||
        !cJSON_AddBoolToObject(work, "independentAssignment", false) ||
        !cJSON_AddStringToObject(work, "poolMode", "failover") ||
        !cJSON_AddNumberToObject(work, "activePool", snapshot->using_fallback ? 1 : 0) ||
        !cJSON_AddArrayToObject(root, "asics") ||
        !cJSON_AddNullToObject(root, "coin") ||
        !cJSON_AddNullToObject(root, "workContext") ||
        !cJSON_AddBoolToObject(mux, "independentWorkAssignment", false)) goto failed;

    const FiveTratumMuxSnapshot *peer = &snapshot->peer;
    const bool current = snapshot->coherent && snapshot->protocol_v1 &&
                         peer->fallback == snapshot->using_fallback;
    const bool connected = current && peer->connected;
    const bool acknowledged = connected && peer->acknowledged && !peer->expired &&
                              peer->age_seconds < FIVE_TRATUM_MUX_TTL_SECONDS;
    const bool expired = connected && peer->expired && !peer->acknowledged;
    const int selected = snapshot->using_fallback ? 1 : 0;
    for (int index = 0; index < 2; ++index) {
        cJSON *pool = cJSON_CreateObject();
        if (!pool) goto failed;
        if (!cJSON_AddItemToArray(pools, pool)) { cJSON_Delete(pool); goto failed; }
        const bool active = index == selected;
        if (!cJSON_AddNumberToObject(pool, "index", index) ||
            !cJSON_AddBoolToObject(pool, "connected", active && acknowledged) ||
            !cJSON_AddBoolToObject(pool, "transportConnected", active && connected) ||
            !cJSON_AddBoolToObject(pool, "expired", active && expired) ||
            !cJSON_AddNullToObject(pool, "serverId") ||
            !cJSON_AddNumberToObject(pool, "ttlSeconds", FIVE_TRATUM_MUX_TTL_SECONDS) ||
            !cJSON_AddNullToObject(pool, "coin") ||
            !cJSON_AddNullToObject(pool, "workContext")) goto failed;
        if (active && (acknowledged || expired)) {
            if (!cJSON_AddNumberToObject(pool, "statusAgeSeconds", peer->age_seconds)) goto failed;
        } else if (!cJSON_AddNullToObject(pool, "statusAgeSeconds")) goto failed;
    }
    return root;
failed:
    cJSON_Delete(root);
    return NULL;
}
