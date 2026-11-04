#!/usr/bin/env python3
"""Exercise production active-web version reporting with bounded fake services."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / 'main/system.c').read_text()
START = SOURCE.index('static bool copy_web_version(')
END = SOURCE.index('\nesp_err_t SYSTEM_init_peripherals(', START)
PRODUCTION = SOURCE[START:END]

HARNESS = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *path; const uint8_t *data; size_t size; bool is_gzipped; } EmbeddedFile;
typedef struct { const char *version; } esp_app_desc_t;
typedef struct { struct { char *version; char *axeOSVersion; } SYSTEM_MODULE; bool filesystem_is_available; } GlobalState;
static const esp_app_desc_t app = { "5tratumFW-0.1.0-beta.15" };
static bool use_custom, have_embedded = true, allocation_always_fails;
static unsigned embedded_reads, custom_reads, allocations, fail_allocation;
static FILE *custom_file;
static EmbeddedFile embedded = { "/version.txt", NULL, 0, false };
static const esp_app_desc_t *esp_app_get_description(void) { return &app; }
#define NVS_CONFIG_USE_CUSTOM_WWW 1
static bool nvs_config_get_bool(int key) { assert(key == 1); return use_custom; }
static const EmbeddedFile *get_embedded_file(const char *path) {
    assert(!strcmp(path, "/version.txt")); ++embedded_reads;
    return have_embedded ? &embedded : NULL;
}
static FILE *host_fopen(const char *path, const char *mode) {
    assert(!strcmp(path, "/version.txt") && !strcmp(mode, "rb"));
    ++custom_reads; return custom_file;
}
static char *host_strdup(const char *text) {
    ++allocations;
    if (allocation_always_fails || allocations == fail_allocation) return NULL;
    const size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    assert(copy); memcpy(copy, text, length); return copy;
}
#define fopen host_fopen
#define strdup host_strdup
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
'''

CASES = r'''
static void set_embedded(const uint8_t *data, size_t size) {
    embedded.data = data; embedded.size = size;
}
static void set_custom(const uint8_t *data, size_t size) {
    custom_file = tmpfile(); assert(custom_file);
    assert(fwrite(data, 1, size, custom_file) == size); rewind(custom_file);
}
#define BYTES(s) (const uint8_t *)(s), sizeof(s) - 1
int main(int argc, char **argv) {
    assert(argc == 2);
    GlobalState state = {0};
    const char *expected = app.version;
    set_embedded((const uint8_t *)app.version, strlen(app.version));
    if (!strcmp(argv[1], "embedded-without-filesystem")) {
        state.filesystem_is_available = false;
    } else if (!strcmp(argv[1], "embedded-ignores-stale-custom")) {
        state.filesystem_is_available = true; set_custom(BYTES("stale-custom-version\n"));
    } else if (!strcmp(argv[1], "custom-unavailable-falls-back-to-embedded")) {
        use_custom = true; state.filesystem_is_available = false;
    } else if (!strcmp(argv[1], "embedded-is-not-firmware-alias")) {
        set_embedded(BYTES("different-embedded-version")); expected = "different-embedded-version";
    } else if (!strcmp(argv[1], "embedded-crlf")) {
        set_embedded(BYTES("5tratumFW-0.1.0-beta.15\r\n"));
    } else if (!strcmp(argv[1], "embedded-missing")) {
        have_embedded = false; expected = "Unknown";
    } else if (!strcmp(argv[1], "embedded-gzip")) {
        embedded.is_gzipped = true; expected = "Unknown";
    } else if (!strcmp(argv[1], "embedded-invalid")) {
        const char *invalid[] = { "", "two\nlines", "embedded space", "tail\r", "line\n\n", "\001bad" };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            set_embedded((const uint8_t *)invalid[i], strlen(invalid[i]));
            SYSTEM_init_versions(&state); assert(!strcmp(state.SYSTEM_MODULE.axeOSVersion, "Unknown"));
            free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        }
        static const uint8_t nul[] = { 'v', '1', 0, 'x' };
        set_embedded(nul, sizeof(nul)); expected = "Unknown";
    } else if (!strcmp(argv[1], "embedded-size-boundaries")) {
        uint8_t text[67]; memset(text, 'x', sizeof(text));
        set_embedded(text, 63); SYSTEM_init_versions(&state);
        assert(strlen(state.SYSTEM_MODULE.axeOSVersion) == 63);
        free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        set_embedded(text, 64); SYSTEM_init_versions(&state);
        assert(!strcmp(state.SYSTEM_MODULE.axeOSVersion, "Unknown"));
        free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        text[63] = '\r'; text[64] = '\n'; set_embedded(text, 65);
        SYSTEM_init_versions(&state); assert(strlen(state.SYSTEM_MODULE.axeOSVersion) == 63);
        free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        set_embedded(NULL, SIZE_MAX); expected = "Unknown";
    } else if (!strcmp(argv[1], "custom-mismatch-is-retained")) {
        use_custom = true; state.filesystem_is_available = true;
        set_custom(BYTES("5tratumFW-0.1.0-beta.14\r\n")); expected = "5tratumFW-0.1.0-beta.14";
    } else if (!strcmp(argv[1], "custom-matching")) {
        use_custom = true; state.filesystem_is_available = true;
        set_custom((const uint8_t *)app.version, strlen(app.version));
    } else if (!strcmp(argv[1], "custom-missing")) {
        use_custom = true; state.filesystem_is_available = true; expected = "Unknown";
    } else if (!strcmp(argv[1], "custom-invalid")) {
        use_custom = true; state.filesystem_is_available = true;
        const char *invalid[] = { "", "v1\nv2", "has space", "bad\r" };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            set_custom((const uint8_t *)invalid[i], strlen(invalid[i]));
            SYSTEM_init_versions(&state); assert(!strcmp(state.SYSTEM_MODULE.axeOSVersion, "Unknown"));
            free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        }
        uint8_t long_text[66]; memset(long_text, 'x', sizeof(long_text));
        set_custom(long_text, sizeof(long_text)); expected = "Unknown";
    } else if (!strcmp(argv[1], "allocation-fallbacks")) {
        fail_allocation = 1; SYSTEM_init_versions(&state);
        assert(!strcmp(state.SYSTEM_MODULE.version, "Unknown"));
        assert(!strcmp(state.SYSTEM_MODULE.axeOSVersion, app.version));
        free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        allocations = 0; fail_allocation = 2; SYSTEM_init_versions(&state);
        assert(!strcmp(state.SYSTEM_MODULE.version, app.version));
        assert(!strcmp(state.SYSTEM_MODULE.axeOSVersion, "Unknown"));
        free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
        allocations = 0; fail_allocation = 0; allocation_always_fails = true;
        SYSTEM_init_versions(&state);
        assert(!state.SYSTEM_MODULE.version && !state.SYSTEM_MODULE.axeOSVersion);
        return 0;
    } else { assert(false); }
    SYSTEM_init_versions(&state);
    assert(!strcmp(state.SYSTEM_MODULE.version, app.version));
    assert(!strcmp(state.SYSTEM_MODULE.axeOSVersion, expected));
    assert(use_custom && state.filesystem_is_available ? embedded_reads == 0 : custom_reads == 0);
    if (custom_file && !custom_reads) fclose(custom_file);
    free(state.SYSTEM_MODULE.version); free(state.SYSTEM_MODULE.axeOSVersion);
    return 0;
}
'''


class SystemWebVersionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='gamma-web-version-')
        cls.addClassCleanup(cls.directory.cleanup)
        source = Path(cls.directory.name) / 'check.c'
        source.write_text(HARNESS + PRODUCTION + CASES)
        cls.binary = Path(cls.directory.name) / 'check'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        str(source), '-o', str(cls.binary)], check=True)

    def run_case(self, case):
        subprocess.run([str(self.binary), case], check=True, capture_output=True)

    def test_embedded_without_filesystem(self): self.run_case('embedded-without-filesystem')
    def test_embedded_does_not_read_stale_custom_version(self): self.run_case('embedded-ignores-stale-custom')
    def test_unavailable_custom_filesystem_uses_actual_embedded_version(self): self.run_case('custom-unavailable-falls-back-to-embedded')
    def test_embedded_version_is_never_an_alias_for_firmware_version(self): self.run_case('embedded-is-not-firmware-alias')
    def test_embedded_crlf_is_trimmed(self): self.run_case('embedded-crlf')
    def test_missing_embedded_marker_is_unknown(self): self.run_case('embedded-missing')
    def test_compressed_embedded_marker_is_unknown(self): self.run_case('embedded-gzip')
    def test_malformed_embedded_marker_is_unknown(self): self.run_case('embedded-invalid')
    def test_embedded_size_boundaries_are_checked(self): self.run_case('embedded-size-boundaries')
    def test_custom_mismatch_is_retained(self): self.run_case('custom-mismatch-is-retained')
    def test_custom_matching_version_is_retained(self): self.run_case('custom-matching')
    def test_missing_custom_marker_is_unknown(self): self.run_case('custom-missing')
    def test_malformed_or_overlong_custom_marker_is_unknown(self): self.run_case('custom-invalid')
    def test_allocation_fallbacks_are_safe(self): self.run_case('allocation-fallbacks')


if __name__ == '__main__':
    unittest.main(verbosity=2)
