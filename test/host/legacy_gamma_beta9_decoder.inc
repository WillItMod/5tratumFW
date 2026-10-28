// Exact pre-change Gamma Beta9 decoder and dependencies, retained as an
// independent downgrade compatibility oracle. GPLv3; upstream attribution is
// retained by the repository. Extracted from the checksum-verified release
// 5tratumFW-0.1.0-beta.9-source.zip (SHA256
// 9d80492c47dd8f6a7dbfa2e8abd797e3cbfd8a91416d04d98b375231c5ef97e5).
// Do not update this fixture when changing the production journal encoder.
#define OPERATING_JOURNAL_MAX 32768
#define OPERATING_JOURNAL_COUNT (NVS_CONFIG_USE_FALLBACK_STRATUM - NVS_CONFIG_STRATUM_PROTOCOL + 4)
#define NVS_STR_LIMIT (4000 - 1)
static bool operating_key(NvsConfigKey key)
{
    return (key >= NVS_CONFIG_STRATUM_PROTOCOL && key <= NVS_CONFIG_USE_FALLBACK_STRATUM) ||
           key == NVS_CONFIG_ASIC_FREQUENCY || key == NVS_CONFIG_ASIC_VOLTAGE || key == NVS_CONFIG_OVERCLOCK_ENABLED;
}

static uint16_t get16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }

static uint32_t get32(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static uint32_t journal_checksum(const uint8_t *p, size_t size)
{
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; i++) value = (value ^ p[i]) * 16777619u;
    return value;
}

static bool mutation_valid(const NvsConfigMutation *entry)
{
    Settings *setting = nvs_config_get_settings(entry->key);
    if (!operating_key(entry->key) || !setting || entry->type != setting->type) return false;
    switch (entry->type) {
    case TYPE_STR: return entry->value.str && strlen(entry->value.str) <= NVS_STR_LIMIT;
    case TYPE_FLOAT: return isfinite(entry->value.f) && entry->value.f >= 1 && entry->value.f <= UINT16_MAX;
    case TYPE_U16: return entry->value.u16 >= setting->min && entry->value.u16 <= setting->max;
    case TYPE_BOOL: return true;
    default: return false;
    }
}

static void mutations_free(NvsConfigMutation *entries, size_t count)
{
    for (size_t i = 0; i < count; i++) if (entries[i].type == TYPE_STR) free(entries[i].value.str);
}

static esp_err_t journal_decode(const uint8_t *blob, size_t size, NvsConfigMutation *entries)
{
    if (size < 12 || size > OPERATING_JOURNAL_MAX || memcmp(blob, "5FWJ", 4) || blob[4] != 1 ||
        blob[5] != OPERATING_JOURNAL_COUNT || get16(blob + 6) != size || get32(blob + 8) != journal_checksum(blob + 12, size - 12)) return ESP_ERR_INVALID_STATE;
    memset(entries, 0, sizeof(*entries) * OPERATING_JOURNAL_COUNT);
    bool seen[NVS_CONFIG_COUNT] = {0}; size_t offset = 12, read = 0;
    esp_err_t error = ESP_ERR_INVALID_STATE;
    while (read < OPERATING_JOURNAL_COUNT) {
        if (offset + 5 > size) goto failed;
        NvsConfigMutation *entry = &entries[read];
        entry->key = get16(blob + offset); entry->type = blob[offset + 2];
        size_t bytes = get16(blob + offset + 3); offset += 5;
        if (!operating_key(entry->key) || entry->key >= NVS_CONFIG_COUNT || seen[entry->key] || offset + bytes > size) goto failed;
        seen[entry->key] = true;
        if (entry->type == TYPE_STR) {
            if (!bytes || bytes > NVS_STR_LIMIT + 1 || blob[offset + bytes - 1] || memchr(blob + offset, 0, bytes - 1)) goto failed;
            entry->value.str = malloc(bytes);
            if (!entry->value.str) { error = ESP_ERR_NO_MEM; goto failed; }
            memcpy(entry->value.str, blob + offset, bytes);
        } else if (entry->type == TYPE_FLOAT && bytes == 4) { uint32_t bits = get32(blob + offset); memcpy(&entry->value.f, &bits, 4); }
        else if ((entry->type == TYPE_U16 || entry->type == TYPE_BOOL) && bytes == 2) {
            uint16_t value = get16(blob + offset);
            if (entry->type == TYPE_BOOL) { if (value > 1) goto failed; entry->value.b = value != 0; }
            else entry->value.u16 = value;
        } else goto failed;
        read++; offset += bytes;
        if (!mutation_valid(entry)) goto failed;
    }
    if (offset != size) goto failed;
    return ESP_OK;
failed:
    mutations_free(entries, read + (read < OPERATING_JOURNAL_COUNT && entries[read].type == TYPE_STR && entries[read].value.str ? 1 : 0));
    return error;
}
