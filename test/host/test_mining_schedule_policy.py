#!/usr/bin/env python3
"""Compile and exercise the production weekly policy without ESP-IDF or NVS."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mining_schedule_policy.h"

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static struct tm at(int day, int minute)
{
    return (struct tm){.tm_wday = day, .tm_hour = minute / 60, .tm_min = minute % 60};
}

static MiningScheduleConfig one(unsigned days, unsigned start, unsigned end)
{
    return (MiningScheduleConfig){.enabled = true, .window_count = 1,
        .windows = {{.days = days, .start_minutes = start, .end_minutes = end}}};
}

static bool paused(const MiningScheduleConfig *config, int day, int minute)
{
    struct tm local = at(day, minute);
    return mining_schedule_is_paused(config, true, &local);
}

static bool evaluate(MiningScheduleEngine *engine, const MiningScheduleConfig *config,
                     bool valid_clock, int day, int minute)
{
    struct tm local = at(day, minute);
    return mining_schedule_engine_evaluate(engine, config, valid_clock, &local);
}

static void validation(void)
{
    MiningScheduleConfig config = one(2, 540, 1020);
    REQUIRE(mining_schedule_config_valid(&config));
    REQUIRE(!mining_schedule_config_valid(NULL));
    config.window_count = 0;
    REQUIRE(!mining_schedule_config_valid(&config));
    config.enabled = false;
    REQUIRE(mining_schedule_config_valid(&config));
    config.enabled = true;
    config.window_count = 8;
    for (unsigned i = 0; i < 8; ++i) config.windows[i] = (MiningScheduleWindow){127, i, i + 1};
    REQUIRE(mining_schedule_config_valid(&config));
    config.window_count = 9;
    REQUIRE(!mining_schedule_config_valid(&config));
    config = one(0, 540, 1020);
    REQUIRE(!mining_schedule_config_valid(&config));
    config = one(128, 540, 1020);
    REQUIRE(!mining_schedule_config_valid(&config));
    config = one(2, 1440, 1020);
    REQUIRE(!mining_schedule_config_valid(&config));
    config = one(2, 540, 1440);
    REQUIRE(!mining_schedule_config_valid(&config));
    config = one(2, 540, 540);
    REQUIRE(!mining_schedule_config_valid(&config));
    config = one(127, 1439, 0);
    REQUIRE(mining_schedule_config_valid(&config));
}

static void boundaries_and_weekdays(void)
{
    MiningScheduleConfig config = one(2, 540, 1020);
    REQUIRE(!paused(&config, 1, 539));
    REQUIRE(paused(&config, 1, 540));
    REQUIRE(paused(&config, 1, 1019));
    REQUIRE(!paused(&config, 1, 1020));
    REQUIRE(!paused(&config, 0, 600));
    REQUIRE(!paused(&config, 2, 600));
    config.windows[0].days = 62;
    for (int day = 0; day < 7; ++day) REQUIRE(paused(&config, day, 600) == (day >= 1 && day <= 5));
    config.windows[0].days = 1;
    REQUIRE(paused(&config, 0, 600));
    REQUIRE(!paused(&config, 6, 600));
}

static void overnight(void)
{
    MiningScheduleConfig config = one(64, 1320, 360);
    REQUIRE(!paused(&config, 6, 1319));
    REQUIRE(paused(&config, 6, 1320));
    REQUIRE(paused(&config, 6, 1439));
    REQUIRE(paused(&config, 0, 0));
    REQUIRE(paused(&config, 0, 359));
    REQUIRE(!paused(&config, 0, 360));
    REQUIRE(!paused(&config, 0, 1320));
    REQUIRE(!paused(&config, 6, 300));
    config.windows[0].days = 2;
    REQUIRE(paused(&config, 1, 1320));
    REQUIRE(paused(&config, 2, 359));
    REQUIRE(!paused(&config, 1, 359));
    config = one(2, 1439, 0);
    REQUIRE(paused(&config, 1, 1439));
    REQUIRE(!paused(&config, 2, 0));
}

/* Independent reference: expand each selected start day into a circular range.
 * This checks every minute of a week, rather than repeating the policy's
 * today/yesterday predicates in the test.
 */
static void reference_week(const MiningScheduleConfig *config, bool expected[10080])
{
    memset(expected, 0, 10080 * sizeof(*expected));
    for (unsigned i = 0; i < config->window_count; ++i) {
        const MiningScheduleWindow window = config->windows[i];
        const int duration = (window.end_minutes - window.start_minutes + 1440) % 1440;
        for (int day = 0; day < 7; ++day) {
            if (!(window.days & (1u << day))) continue;
            for (int offset = 0; offset < duration; ++offset) {
                expected[(day * 1440 + window.start_minutes + offset) % 10080] = true;
            }
        }
    }
}

static int reference_next(const bool expected[10080], int current)
{
    for (int distance = 1; distance <= 10080; ++distance) {
        const int point = (current + distance) % 10080;
        if (expected[point] != expected[(point + 10079) % 10080]) return distance;
    }
    return -1;
}

static void union_and_next_boundary(void)
{
    MiningScheduleConfig config = {.enabled = true, .window_count = 8, .windows = {
        {2, 540, 720}, {2, 660, 900}, {2, 900, 960}, {64, 1320, 360},
        {1, 240, 600}, {62, 1200, 30}, {8, 700, 710}, {127, 45, 46},
    }};
    bool expected[10080];
    reference_week(&config, expected);
    for (int minute = 0; minute < 10080; ++minute) {
        REQUIRE(paused(&config, minute / 1440, minute % 1440) == expected[minute]);
    }
    const int checkpoints[] = {0, 44, 45, 46, 1440 + 539, 1440 + 540, 1440 + 700,
        1440 + 959, 1440 + 960, 6 * 1440 + 1320, 10079};
    for (unsigned i = 0; i < sizeof(checkpoints) / sizeof(checkpoints[0]); ++i) {
        const int minute = checkpoints[i];
        struct tm local = at(minute / 1440, minute % 1440);
        REQUIRE(mining_schedule_next_boundary_minutes(&config, &local) == reference_next(expected, minute));
    }
    /* Overlap and adjacent starts/ends do not expire an override. */
    struct tm local = at(1, 540);
    REQUIRE(mining_schedule_next_boundary_minutes(&config, &local) == 420);
    config = (MiningScheduleConfig){.enabled = true, .window_count = 2,
        .windows = {{127, 0, 720}, {127, 720, 0}}};
    REQUIRE(mining_schedule_next_boundary_minutes(&config, &local) == -1);
    REQUIRE(paused(&config, 0, 0));
    REQUIRE(paused(&config, 6, 1439));
    config.window_count = 0;
    REQUIRE(paused(&config, 1, 540));
    REQUIRE(mining_schedule_next_boundary_minutes(&config, &local) == -1);
    config.enabled = false;
    REQUIRE(mining_schedule_next_boundary_minutes(&config, &local) == -1);
}

static void clock_invalid(void)
{
    MiningScheduleConfig config = one(2, 540, 1020);
    MiningScheduleEngine engine;
    mining_schedule_engine_init(&engine, false);
    REQUIRE(evaluate(&engine, &config, true, 1, 600));
    mining_schedule_engine_set_override(&engine, false);
    REQUIRE(!evaluate(&engine, &config, true, 1, 601));
    REQUIRE(evaluate(&engine, &config, false, 1, 602));
    REQUIRE(engine.manual_override_active);
    REQUIRE(evaluate(&engine, &config, false, 1, 603));
    REQUIRE(!evaluate(&engine, &config, true, 1, 604));
    REQUIRE(engine.manual_override_active);
    REQUIRE(evaluate(&engine, &config, true, 7, 600));
    REQUIRE(evaluate(&engine, &config, true, 1, 1440));
    struct tm local = at(1, 600);
    local.tm_min = -1;
    REQUIRE(mining_schedule_is_paused(&config, true, &local));
    REQUIRE(mining_schedule_is_paused(&config, true, NULL));
    REQUIRE(mining_schedule_next_boundary_minutes(&config, NULL) == -1);
    REQUIRE(!evaluate(&engine, &config, true, 1, 1020));
    REQUIRE(!engine.manual_override_active);
    config.window_count = 0;
    REQUIRE(evaluate(&engine, &config, false, 1, 600));
    REQUIRE(evaluate(&engine, &config, true, 1, 600));
}

static void overrides(void)
{
    MiningScheduleConfig config = {.enabled = true, .window_count = 2,
        .windows = {{2, 540, 720}, {2, 660, 900}}};
    MiningScheduleEngine engine;
    mining_schedule_engine_init(&engine, false);
    REQUIRE(evaluate(&engine, &config, true, 1, 540));
    mining_schedule_engine_set_override(&engine, false);
    REQUIRE(!evaluate(&engine, &config, true, 1, 550));
    REQUIRE(!evaluate(&engine, &config, true, 1, 660));
    REQUIRE(engine.manual_override_active);
    REQUIRE(!evaluate(&engine, &config, true, 1, 720));
    REQUIRE(engine.manual_override_active);
    REQUIRE(!evaluate(&engine, &config, true, 1, 900));
    REQUIRE(!engine.manual_override_active);
    mining_schedule_engine_set_override(&engine, true);
    REQUIRE(evaluate(&engine, &config, true, 1, 1000));
    REQUIRE(evaluate(&engine, &config, true, 2, 539));
    REQUIRE(engine.manual_override_active);
    REQUIRE(evaluate(&engine, &config, true, 1, 540));
    REQUIRE(!engine.manual_override_active);
    mining_schedule_engine_set_override(&engine, false);
    REQUIRE(!evaluate(&engine, &config, true, 1, 600));
    mining_schedule_engine_clear_override(&engine);
    REQUIRE(evaluate(&engine, &config, true, 1, 601));
    /* A valid clock jump that changes the scheduled region expires override. */
    mining_schedule_engine_set_override(&engine, false);
    REQUIRE(!evaluate(&engine, &config, true, 1, 602));
    REQUIRE(!evaluate(&engine, &config, true, 1, 1000));
    REQUIRE(!engine.manual_override_active);
}

static void disable_latches_state(void)
{
    MiningScheduleConfig config = one(2, 540, 1020);
    MiningScheduleEngine engine;
    mining_schedule_engine_init(&engine, false);
    REQUIRE(evaluate(&engine, &config, true, 1, 600));
    config.enabled = false;
    REQUIRE(evaluate(&engine, &config, true, 1, 1200));
    REQUIRE(evaluate(&engine, &config, false, 2, 1200));
    mining_schedule_engine_set_override(&engine, false);
    REQUIRE(!evaluate(&engine, &config, false, 2, 1200));
    mining_schedule_engine_clear_override(&engine);
    REQUIRE(!evaluate(&engine, &config, true, 2, 1200));
    mining_schedule_engine_init(&engine, true);
    REQUIRE(evaluate(&engine, &config, false, 2, 1200));
    mining_schedule_engine_init(&engine, false);
    REQUIRE(!evaluate(&engine, &config, false, 2, 1200));
}

static void volatile_only(void)
{
    const MiningScheduleConfig config = one(2, 540, 1020);
    const MiningScheduleConfig original = config;
    MiningScheduleEngine engine;
    mining_schedule_engine_init(&engine, false);
    for (int minute = 0; minute < 20160; ++minute) {
        evaluate(&engine, &config, true, (minute / 1440) % 7, minute % 1440);
    }
    REQUIRE(memcmp(&config, &original, sizeof(config)) == 0);
    mining_schedule_engine_set_override(&engine, true);
    REQUIRE(evaluate(&engine, &config, true, 0, 100));
    /* A fresh volatile engine has no stored override or configuration writes. */
    mining_schedule_engine_init(&engine, false);
    REQUIRE(!engine.manual_override_active);
    REQUIRE(!evaluate(&engine, &config, true, 0, 100));
}

int main(int argc, char **argv)
{
    REQUIRE(argc == 2);
    if (!strcmp(argv[1], "validation")) validation();
    else if (!strcmp(argv[1], "boundaries")) boundaries_and_weekdays();
    else if (!strcmp(argv[1], "overnight")) overnight();
    else if (!strcmp(argv[1], "union")) union_and_next_boundary();
    else if (!strcmp(argv[1], "clock")) clock_invalid();
    else if (!strcmp(argv[1], "overrides")) overrides();
    else if (!strcmp(argv[1], "disabled")) disable_latches_state();
    else if (!strcmp(argv[1], "volatile")) volatile_only();
    else REQUIRE(false);
    puts("PASS");
    return 0;
}
'''


class MiningSchedulePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="5tratumfw-schedule-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        harness = directory / "schedule_test.c"
        harness.write_text(HARNESS)
        cls.binary = directory / "schedule-policy-test"
        # Deliberately link no ESP-IDF/NVS implementation or stubs. Any flash
        # dependency introduced into the production policy fails this build.
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "main"),
            str(ROOT / "main/mining_schedule_policy.c"), str(harness), "-o", str(cls.binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"Schedule policy host compilation failed:\n{result.stderr}")

    def run_case(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        self.assertEqual(result.stdout.strip(), "PASS")

    def test_configuration_limits(self):
        self.run_case("validation")

    def test_inclusive_start_exclusive_end_and_weekday_masks(self):
        self.run_case("boundaries")

    def test_overnight_start_days_and_saturday_to_sunday(self):
        self.run_case("overnight")

    def test_union_for_every_week_minute_and_actual_next_boundary(self):
        self.run_case("union")

    def test_invalid_clock_never_resumes_or_consumes_override(self):
        self.run_case("clock")

    def test_manual_overrides_expire_at_union_boundaries_and_clear_explicitly(self):
        self.run_case("overrides")

    def test_disabling_preserves_current_pause_state_and_manual_requests(self):
        self.run_case("disabled")

    def test_policy_uses_only_volatile_state_without_persistence(self):
        self.run_case("volatile")


if __name__ == "__main__":
    unittest.main(verbosity=2)
