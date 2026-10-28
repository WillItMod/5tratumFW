#!/usr/bin/env python3
"""Run production mDNS startup/update and conflict resolution at mocked SDK boundaries.

Discovery aliases must never change the configured hostname. The existing
display/PATCH host regression separately exercises the real explicit save path.
No network or device access occurs in this test.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


class GammaMdnsHostnameTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="5tfw-mdns-hostname-")
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name)
        source = (ROOT / "components/connect/connect.c").read_text()
        harness = r'''
#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#define NVS_CONFIG_HOSTNAME 1
#define WIFI_IF_STA 1
#define MDNS_TYPE_A 1
#define MDNS_QUERY_MULTICAST 2
#define IPADDR_TYPE_V4 0
#define IPADDR_TYPE_V6 6
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) ((const uint8_t *)&(ip)->addr)[0], ((const uint8_t *)&(ip)->addr)[1], ((const uint8_t *)&(ip)->addr)[2], ((const uint8_t *)&(ip)->addr)[3]
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
typedef int esp_err_t;
typedef struct { uint32_t addr; } ip4_addr_t;
typedef struct { ip4_addr_t ip; } esp_netif_ip_info_t;
typedef struct { int index; } esp_netif_t;
typedef struct { const char *version; } esp_app_desc_t;
typedef struct {
    struct { char mdns_hostname[64], full_hostname[72]; } SYSTEM_MODULE;
    struct {
        const char *board_version;
        struct { const char *name; unsigned asic_count; struct { const char *name; } asic; } family;
    } DEVICE_CONFIG;
} GlobalState;
typedef struct mdns_ip_addr {
    struct { int type; union { ip4_addr_t ip4; struct in6_addr ip6; } u_addr; } addr;
} mdns_ip_addr_t;
typedef struct { mdns_ip_addr_t *addr; } mdns_result_t;
enum { NO_RECORD, SELF_RECORD, FOREIGN_RECORD, QUERY_ERROR, NO_ADDRESS, FOREIGN_IPV6, ERROR_WITH_RESULT };
static const char *TAG = "mdns-hostname-fixture";
static bool mdns_initialized;
static char stored_hostname[64] = "bitaxe", advertised_hostname[64], queried_hostname[64];
static unsigned setting_writes, settings_reads, queries, freed_results, init_calls, hostname_calls, services, txt_records;
static int query_mode, init_result, hostname_result, ip_result;
static bool missing_hostname, sta_available = true, ap_available = true;
static esp_netif_t sta = {1}, ap = {2};
static ip4_addr_t own_ip;

static size_t fixture_strlcpy(char *dst, const char *src, size_t size) {
    size_t length = strlen(src);
    if (size) snprintf(dst, size, "%s", src);
    return length;
}
#define strlcpy fixture_strlcpy
char *nvs_config_get_string(int key) {
    assert(key == NVS_CONFIG_HOSTNAME); ++settings_reads;
    return missing_hostname ? NULL : strdup(stored_hostname);
}
// This remains callable so the old production code compiles and fails its
// behavioral assertions if either automatic persistence is reintroduced.
void nvs_config_set_string(int key, const char *value) {
    assert(key == NVS_CONFIG_HOSTNAME && value); ++setting_writes;
    strlcpy(stored_hostname, value, sizeof(stored_hostname));
}
static esp_err_t mdns_init(void) { ++init_calls; return init_result; }
static esp_err_t mdns_hostname_set(const char *value) {
    ++hostname_calls;
    if (hostname_result == ESP_OK) strlcpy(advertised_hostname, value, sizeof(advertised_hostname));
    return hostname_result;
}
static esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key) {
    if (!strcmp(key, "WIFI_STA_DEF")) return sta_available ? &sta : NULL;
    assert(!strcmp(key, "WIFI_AP_DEF")); return ap_available ? &ap : NULL;
}
// This existing fixture models the Wi-Fi-only network boundary. Ethernet
// selection/fallback is exercised by the separate production policy harness.
static esp_netif_t *network_active_netif(void) {
    esp_netif_t *active = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    return active ? active : esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
}
static esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *info) {
    assert(netif == &sta || netif == &ap); info->ip = own_ip; return ip_result;
}
static esp_err_t esp_wifi_get_mac(int interface, uint8_t *mac) {
    assert(interface == WIFI_IF_STA);
    const uint8_t fixture_mac[6] = {0x02, 0, 0, 0, 0xab, 0xcd};
    memcpy(mac, fixture_mac, sizeof(fixture_mac)); return ESP_OK;
}
static const char *esp_ip4addr_ntoa(const ip4_addr_t *ip, char *buffer, size_t size) {
    return inet_ntop(AF_INET, &ip->addr, buffer, (socklen_t)size);
}
static esp_err_t mdns_query_generic(const char *name, const char *service, const char *protocol,
                                  int type, int mode, unsigned timeout, unsigned max_results, mdns_result_t **out) {
    assert(!service && !protocol && type == MDNS_TYPE_A && mode == MDNS_QUERY_MULTICAST);
    assert(timeout == 1000 && max_results == 1); ++queries;
    strlcpy(queried_hostname, name, sizeof(queried_hostname)); *out = NULL;
    if (query_mode == NO_RECORD || query_mode == QUERY_ERROR) return query_mode == QUERY_ERROR ? ESP_FAIL : ESP_OK;
    *out = calloc(1, sizeof(**out)); assert(*out);
    if (query_mode != NO_ADDRESS) {
        (*out)->addr = calloc(1, sizeof(*(*out)->addr)); assert((*out)->addr);
        if (query_mode == FOREIGN_IPV6) {
            (*out)->addr->addr.type = IPADDR_TYPE_V6;
            assert(inet_pton(AF_INET6, "2001:db8::2", &(*out)->addr->addr.u_addr.ip6) == 1);
        } else {
            (*out)->addr->addr.type = IPADDR_TYPE_V4;
            assert(inet_pton(AF_INET, query_mode == SELF_RECORD ? "192.0.2.10" : "192.0.2.11",
                             &(*out)->addr->addr.u_addr.ip4.addr) == 1);
        }
    }
    return query_mode == ERROR_WITH_RESULT ? ESP_FAIL : ESP_OK;
}
static void mdns_query_results_free(mdns_result_t *results) {
    assert(results); ++freed_results; free(results->addr); free(results);
}
static esp_err_t mdns_service_add(const char *instance, const char *service, const char *protocol,
                                  int port, const void *txt, size_t count) {
    assert(!strcmp(instance, "Bitaxe Gamma 602 (ABCD)"));
    assert(!strcmp(service, "_http") && !strcmp(protocol, "_tcp") && port == 80 && !txt && !count);
    ++services; return ESP_OK;
}
static esp_err_t mdns_service_subtype_add_for_host(const char *instance, const char *service, const char *protocol,
                                                 const char *host, const char *subtype) {
    assert(instance && !strcmp(service, "_http") && !strcmp(protocol, "_tcp") && !host && !strcmp(subtype, "_axeos"));
    return ESP_OK;
}
static esp_err_t mdns_service_txt_item_set_for_host(const char *instance, const char *service, const char *protocol,
                                                  const char *host, const char *key, const char *value) {
    assert(instance && !strcmp(service, "_http") && !strcmp(protocol, "_tcp") && !host && key && value);
    ++txt_records; return ESP_OK;
}
static const esp_app_desc_t *esp_app_get_description(void) {
    static const esp_app_desc_t app = {"fixture-version"}; return &app;
}
'''
        for signature in (
            "static char* generate_unique_hostname(const char *base) {",
            "static char* check_and_resolve_hostname_conflict(const char *hostname, const char *current_ip) {",
            "static void initialize_mdns_if_needed(GlobalState *GLOBAL_STATE) {",
            "esp_err_t update_mdns_hostname(const char *new_hostname, GlobalState *GLOBAL_STATE) {",
        ):
            harness += method(source, signature)
        harness += r'''
static GlobalState state;
static void setup(int mode) {
    memset(&state, 0, sizeof(state));
    state.DEVICE_CONFIG.board_version = "602";
    state.DEVICE_CONFIG.family.name = "Gamma";
    state.DEVICE_CONFIG.family.asic.name = "BM1370";
    state.DEVICE_CONFIG.family.asic_count = 1;
    strlcpy(stored_hostname, "bitaxe", sizeof(stored_hostname));
    advertised_hostname[0] = queried_hostname[0] = 0;
    setting_writes = settings_reads = queries = freed_results = init_calls = hostname_calls = services = txt_records = 0;
    mdns_initialized = missing_hostname = false; sta_available = ap_available = true;
    init_result = hostname_result = ip_result = ESP_OK; query_mode = mode;
    assert(inet_pton(AF_INET, "192.0.2.10", &own_ip.addr) == 1);
}
static void check_alias(const char *base, const char *alias, unsigned expected_writes) {
    char full[72]; snprintf(full, sizeof(full), "%s.local", alias);
    assert(!strcmp(stored_hostname, base)); assert(setting_writes == expected_writes);
    assert(!strcmp(advertised_hostname, alias));
    assert(!strcmp(state.SYSTEM_MODULE.mdns_hostname, alias));
    assert(!strcmp(state.SYSTEM_MODULE.full_hostname, full));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const int scenario = atoi(argv[1]);
    if (scenario == 0) {
        const int modes[] = {NO_RECORD, SELF_RECORD, QUERY_ERROR, NO_ADDRESS, ERROR_WITH_RESULT};
        for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
            setup(modes[i]); initialize_mdns_if_needed(&state);
            check_alias("bitaxe", "bitaxe", 0);
            assert(mdns_initialized && init_calls == 1 && queries == 1 && services == 1 && txt_records == 5);
            assert(freed_results == (modes[i] == SELF_RECORD || modes[i] == NO_ADDRESS || modes[i] == ERROR_WITH_RESULT));
            initialize_mdns_if_needed(&state);
            assert(init_calls == 1 && settings_reads == 1 && queries == 1 && setting_writes == 0);
        }
    } else if (scenario == 1) {
        setup(FOREIGN_RECORD); initialize_mdns_if_needed(&state);
        check_alias("bitaxe", "bitaxe-abcd", 0);
        assert(!strcmp(queried_hostname, "bitaxe") && freed_results == 1 && services == 1);
        // A subsequent boot must query the configured base again, without an
        // accumulating suffix or a hidden persistent settings mutation.
        mdns_initialized = false; memset(&state.SYSTEM_MODULE, 0, sizeof(state.SYSTEM_MODULE));
        initialize_mdns_if_needed(&state); check_alias("bitaxe", "bitaxe-abcd", 0);
        assert(!strcmp(queried_hostname, "bitaxe") && freed_results == 2 && init_calls == 2);
    } else if (scenario == 2) {
        setup(FOREIGN_RECORD); missing_hostname = true; initialize_mdns_if_needed(&state);
        assert(!mdns_initialized && !init_calls && !queries && !setting_writes);
        setup(FOREIGN_RECORD); init_result = ESP_FAIL; initialize_mdns_if_needed(&state);
        assert(!mdns_initialized && init_calls == 1 && !queries && !setting_writes);
        setup(FOREIGN_RECORD); hostname_result = ESP_FAIL;
        strlcpy(state.SYSTEM_MODULE.mdns_hostname, "previous", sizeof(state.SYSTEM_MODULE.mdns_hostname));
        strlcpy(state.SYSTEM_MODULE.full_hostname, "previous.local", sizeof(state.SYSTEM_MODULE.full_hostname));
        initialize_mdns_if_needed(&state);
        assert(mdns_initialized && freed_results == 1 && services == 1 && !setting_writes && !strcmp(stored_hostname, "bitaxe"));
        assert(!strcmp(state.SYSTEM_MODULE.mdns_hostname, "previous") && !strcmp(state.SYSTEM_MODULE.full_hostname, "previous.local"));
    } else if (scenario == 3) {
        setup(FOREIGN_RECORD); assert(update_mdns_hostname("bitaxe", &state) == ESP_OK);
        check_alias("bitaxe", "bitaxe-abcd", 0);
        assert(update_mdns_hostname("bitaxe", &state) == ESP_OK);
        check_alias("bitaxe", "bitaxe-abcd", 0); assert(freed_results == 2);
        // Preserve the HTTP caller's explicitly saved base name. Its one
        // settings write must not be followed by an automatic alias write.
        nvs_config_set_string(NVS_CONFIG_HOSTNAME, "requested-name");
        assert(update_mdns_hostname("requested-name", &state) == ESP_OK);
        check_alias("requested-name", "requested-name-abcd", 1);
        assert(!strcmp(queried_hostname, "requested-name") && !settings_reads);
    } else if (scenario == 4) {
        const int modes[] = {NO_RECORD, SELF_RECORD, QUERY_ERROR, NO_ADDRESS, ERROR_WITH_RESULT};
        for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
            setup(modes[i]); assert(update_mdns_hostname("bitaxe", &state) == ESP_OK);
            check_alias("bitaxe", "bitaxe", 0);
            assert(!init_calls && !services && !settings_reads && queries == 1);
            assert(freed_results == (modes[i] == SELF_RECORD || modes[i] == NO_ADDRESS || modes[i] == ERROR_WITH_RESULT));
        }
    } else if (scenario == 5) {
        setup(FOREIGN_RECORD);
        assert(update_mdns_hostname(NULL, &state) == ESP_ERR_INVALID_ARG);
        assert(update_mdns_hostname("", &state) == ESP_ERR_INVALID_ARG);
        assert(!queries && !hostname_calls && !setting_writes);
        strlcpy(state.SYSTEM_MODULE.mdns_hostname, "previous", sizeof(state.SYSTEM_MODULE.mdns_hostname));
        strlcpy(state.SYSTEM_MODULE.full_hostname, "previous.local", sizeof(state.SYSTEM_MODULE.full_hostname));
        hostname_result = ESP_FAIL;
        assert(update_mdns_hostname("bitaxe", &state) == ESP_FAIL);
        assert(!strcmp(state.SYSTEM_MODULE.mdns_hostname, "previous") && !strcmp(state.SYSTEM_MODULE.full_hostname, "previous.local"));
        assert(!setting_writes && !strcmp(stored_hostname, "bitaxe") && freed_results == 1);
        hostname_result = ESP_OK;
        assert(update_mdns_hostname("bitaxe", NULL) == ESP_OK);
        assert(!strcmp(advertised_hostname, "bitaxe-abcd") && !setting_writes && freed_results == 2);
    } else if (scenario == 6) {
        // AP fallback, no interface, and failed address lookup must still keep
        // the saved name while resolving an independently advertised alias.
        for (unsigned i = 0; i < 4; ++i) {
            setup(FOREIGN_IPV6); sta_available = false;
            if (i == 1) ap_available = false;
            if (i == 2) ip_result = ESP_FAIL;
            if (i == 3) own_ip.addr = 0;
            initialize_mdns_if_needed(&state); check_alias("bitaxe", "bitaxe-abcd", 0);
            assert(update_mdns_hostname("bitaxe", &state) == ESP_OK);
            check_alias("bitaxe", "bitaxe-abcd", 0); assert(freed_results == 2);
        }
    } else assert(!"Unknown fixture scenario");
    return 0;
}
'''
        fixture = directory / "mdns-hostname.c"
        fixture.write_text(harness)
        cls.binary = directory / "mdns-hostname"
        subprocess.run([shutil.which("clang") or shutil.which("cc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", str(fixture), "-o", str(cls.binary)], check=True)

    def run_scenario(self, number):
        subprocess.run([str(self.binary), str(number)], check=True)

    def test_startup_without_foreign_conflict(self):
        self.run_scenario(0)

    def test_startup_conflict_and_subsequent_boot_preserve_saved_name(self):
        self.run_scenario(1)

    def test_startup_errors_never_persist_alias(self):
        self.run_scenario(2)

    def test_update_conflict_preserves_explicitly_saved_base_name(self):
        self.run_scenario(3)

    def test_update_without_foreign_conflict(self):
        self.run_scenario(4)

    def test_invalid_update_and_set_failure_preserve_settings_and_runtime(self):
        self.run_scenario(5)

    def test_ipv6_conflict_with_network_fallbacks(self):
        self.run_scenario(6)


if __name__ == "__main__":
    unittest.main()
