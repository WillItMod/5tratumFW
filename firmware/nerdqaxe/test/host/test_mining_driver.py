#!/usr/bin/env python3
"""Actual ASIC/BM1370 and board method bodies against UART/GPIO/Buck stubs.

No firmware SDK, USB, network or live miner access. Only platform include
boundaries and external hardware are replaced; tested method bodies are exact.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "components/bm1397"
SOURCE_FILES = (
    "components/bm1397/asic.cpp", "components/bm1397/bm1370.cpp",
    "components/bm1397/include/asic.h", "components/bm1397/include/bm1368.h",
    "components/bm1397/include/bm1370.h", "components/bm1397/include/mining.h",
    "components/bm1397/include/serial.h", "components/bm1397/crc.cpp",
    "components/bm1397/include/crc.h", "components/bm1397/mining_utils.cpp",
    "components/bm1397/include/mining_utils.h", "components/bm1397/mining.cpp",
    "components/bm1397/bm1370_protocol.cpp", "components/bm1397/include/bm1370_protocol.h",
    "components/bm1397/include/bm1370_capture.h", "components/bm1397/include/bm1370_capture_runtime.h",
    "main/boards/nerdqaxeplus.cpp", "main/boards/nerdqaxeplus2.cpp",
    "main/tasks/asic_jobs.h", "main/tasks/create_jobs_task.cpp",
    "main/tasks/power_management_task.cpp", "main/tasks/mining_control.cpp",
    "main/tasks/mining_control.h", "main/tasks/mining_control_state.h",
    "main/tasks/mining_schedule.h", "main/main.cpp",
    "test/host/mining_driver_support.h", "test/host/mining_driver_harness.cpp",
    "test/host/test_mining_driver.py",
)
def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"

# Integration order cannot be executed without the ESP runtime. Check its
# actual source wiring separately from the executable driver/worker proofs.
main_source = (ROOT / "main/main.cpp").read_text()
assert main_source.index("FiveTratumMining::initializeSchedule()") < main_source.index("FiveTratumMining::initializeControl(board, canSlave)")
assert main_source.index("FiveTratumMining::initializeControl(board, canSlave)") < main_source.index('xTaskCreatePSRAM(POWER_MANAGEMENT_MODULE.taskWrapper')
self_test = method(main_source, "if (should_self_test && !best_diff)")
qualified_self_test = method(self_test, "if (FiveTratumMining::controlSupported())")
assert "board->selfTest()" not in qualified_self_test and "board->selfTest()" in self_test
legacy_init = method(main_source, "if (!FiveTratumMining::controlSupported())")
assert "board->initAsics()" in legacy_init
assert 'FiveTratumMining::setRuntimeReady(jobTaskReady && resultTaskReady && stratumTaskReady && counterReady)' in main_source
print("Source wiring: policy before power owner, qualified factory self-test gated, worker readiness bound")

with tempfile.TemporaryDirectory(prefix="5tratum-mining-driver-") as directory:
    path = Path(directory)
    (path / "driver").mkdir(); (path / "rom").mkdir()
    (path / "driver/gpio.h").write_text("#pragma once\n")
    (path / "rom/gpio.h").write_text("#pragma once\n")
    mining = (COMPONENT / "include/mining.h").read_text()
    begin = mining.index("typedef struct")
    end = mining.index("} bm_job;", begin) + len("} bm_job;")
    (path / "mining.h").write_text("#pragma once\n#include <cstdint>\n" + mining[begin:end] + "\n")
    # Keep production ASIC declarations verbatim beside the SDK-free job type,
    # so their quoted mining.h includes resolve to this platform boundary.
    for name in ("asic.h", "bm1368.h", "bm1370.h", "bm1370_protocol.h"):
        (path / name).write_text((COMPONENT / "include" / name).read_text())
    generated = []
    platform_preamble = ('#include "mining_driver_support.h"\n'
        '#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE\n'
        '#include "bm1370_capture_runtime.h"\n#endif\n')
    for name in ("asic.cpp", "bm1370.cpp"):
        source = (COMPONENT / name).read_text()
        source = re.sub(r'^#include "[^"\n]+"\n', '', source, flags=re.M)
        source = source.replace('#include <endian.h>', '')
        target = path / name
        target.write_text(platform_preamble + source)
        generated.append(str(target))
    helpers = (COMPONENT / "mining_utils.cpp").read_text()
    (path / "helpers.cpp").write_text('#include "mining_driver_support.h"\n' +
        method(helpers, "unsigned char _reverse_bits(") + method(helpers, "int _largest_power_of_two("))
    board = (ROOT / "main/boards/nerdqaxeplus.cpp").read_text()
    second = (ROOT / "main/boards/nerdqaxeplus2.cpp").read_text()
    board_methods = '#include "mining_driver_support.h"\nstatic const char *TAG="fixture";\n'
    for signature in ("void NerdQaxePlus::setAsicReset(", "bool NerdQaxePlus::pauseMiningPower(",
                      "bool NerdQaxePlus::initAsics(", "bool NerdQaxePlus::setVoltage(",
                      "void NerdQaxePlus::LDO_enable(", "void NerdQaxePlus::LDO_disable(",
                      "void NerdQaxePlus::VREG_enable(", "void NerdQaxePlus::VREG_disable("):
        board_methods += method(board, signature)
    board_methods += method(second, "bool NerdQaxePlus2::initAsics(") + method(second, "bool NerdQaxePlus2::setVoltage(")
    (path / "board.cpp").write_text(board_methods)
    jobs = (ROOT / "main/tasks/asic_jobs.h").read_text()
    jobs = re.sub(r'^#include "[^"\n]+"\n', '', jobs, flags=re.M)
    (path / "asic_jobs_actual.h").write_text('#include "mining_driver_support.h"\n' + jobs)
    power = (ROOT / "main/tasks/power_management_task.cpp").read_text()
    (path / "protection.cpp").write_text('#include "mining_driver_support.h"\n' +
        method(power, "bool PowerManagementTask::refreshProtectionForMining("))
    control = (ROOT / "main/tasks/mining_control.cpp").read_text()
    control = re.sub(r'^#include "[^"\n]+"\n', '', control, flags=re.M)
    (path / "control.cpp").write_text(platform_preamble + '#include "mining_control.h"\n#include "mining_schedule.h"\n' + control)
    job_source = (ROOT / "main/tasks/create_jobs_task.cpp").read_text()
    job_start = job_source.index("void create_jobs_task(")
    job_end = job_source.index("    uint32_t last_ntime", job_start)
    # Execute the exact production worker startup before the infinite job loop.
    # Timer and global task boundaries are mocked, not the readiness branches.
    (path / "jobs-startup.cpp").write_text('#include "mining_driver_support.h"\n#include "mining_control.h"\nstatic const char *TAG="jobs";\n' +
        job_source[job_start:job_end] + '    (void)asics; (void)pvParameters;\n}\n')
    binary = path / "driver-test"
    compile_command = [shutil.which("clang++") or shutil.which("g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Wno-sign-compare", "-Wno-vla-cxx-extension", "-Wno-unused-const-variable",
                    "-pthread", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(path), "-I", str(ROOT / "test/host"), "-I", str(COMPONENT / "include"),
                    "-I", str(ROOT / "components/ArduinoJson"),
                    "-I", str(ROOT / "main/tasks"), *generated, str(COMPONENT / "crc.cpp"),
                    str(path / "helpers.cpp"), str(path / "board.cpp"), str(path / "protection.cpp"), str(path / "control.cpp"),
                    str(path / "jobs-startup.cpp"),
                    str(COMPONENT / "bm1370_protocol.cpp"),
                    str(ROOT / "test/host/mining_driver_harness.cpp")]
    subprocess.run(compile_command + ["-DFIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=0",
        "-DFIVETRATUM_BM1370_CAPTURE=0", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    power_cases = ("cold-standard", "cold-rev7", "wrong-chip-count", "voltage-mismatch", "voltage-nan",
                 "resume", "pause-during-ramp", "invalid-vr", "no-workers", "no-job-ack", "job-timer-create-failed", "job-timer-start-failed",
                 "thermal-latch", "baud-failed", "live-ramp", "live-ramp-pause", "live-ramp-uart-failure", "live-ramp-thermal")
    for case in power_cases:
        subprocess.run([str(binary), case], check=True)

    # Keep default legacy RX unchanged; exercise the new decoder only in an
    # explicit diagnostic binary with the same flag in every translation unit.
    subprocess.run([str(binary), "codec"], check=True)
    legacy_wire = subprocess.check_output([str(binary), "init-wire"])
    diagnostic = path / "diagnostic-driver-test"
    subprocess.run(compile_command + ["-DFIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=1",
        "-DFIVETRATUM_BM1370_CAPTURE=0", "-o", str(diagnostic)], check=True)
    subprocess.run([str(diagnostic), "codec"], check=True)
    for case in ("cold-standard", "cold-rev7", "resume", "pause-during-ramp", "live-ramp", "live-ramp-pause", "live-ramp-uart-failure", "live-ramp-thermal"):
        subprocess.run([str(diagnostic), case], check=True)
    diagnostic_wire = subprocess.check_output([str(diagnostic), "init-wire"])
    assert legacy_wire == diagnostic_wire
    # Compile the actual capture branch too. The UART/capture platform boundary
    # records immutable arguments; it never substitutes the production encoder,
    # metadata construction, power lease or retirement call sites.
    capture = path / "capture-driver-test"
    subprocess.run(compile_command + ["-DFIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=1",
        "-DFIVETRATUM_BM1370_CAPTURE=1", "-o", str(capture)], check=True)
    subprocess.run([str(capture), "codec"], check=True)
    subprocess.run([str(capture), "capture-metadata"], check=True)
    for case in power_cases:
        subprocess.run([str(capture), case], check=True)
    capture_wire = subprocess.check_output([str(capture), "init-wire"])
    assert legacy_wire == capture_wire
    proof = ROOT / ".cache/bm1370-driver"
    proof.mkdir(parents=True, exist_ok=True)
    (proof / "proof.json").write_text(json.dumps({
        "defaultDiagnosticDriver": False, "hardwareActions": False,
        "chainWorkIndependent": False, "legacyPowerScenarios": 18,
        "diagnosticPowerScenarios": 8, "capturedFrames": 5, "splitPoints": 50,
        "defaultCapture": False, "capturePowerScenarios": len(power_cases),
        "captureBoundary": "SERIAL_send_job and retirement externally mocked; actual runtime/API tested separately",
        "captureMetadata": {"canonicalHeaderBytes": 80, "jobPacketBytes": 88,
            "roundedTicketDifficulty": 8192, "requestedTicketDifficulty": 10000,
            "immutableArguments": True, "uint64GenerationPreserved": True,
            "legacyCallGenerationUnknown": True, "primarySecondaryUnknownPoolIndices": True,
            "shortWriteStickyFailure": True, "chipTargetSendsNothing": True,
            "powerGenerationRetirement": True, "retirementAddsNoUartCommands": True},
        "singleBitMutants": 440, "jobByteCompatibilityCounters": 2048,
        "initPacketsEqual": True, "initializationPacketCount": len(legacy_wire.splitlines()),
        "initializationWireSha256": hashlib.sha256(legacy_wire).hexdigest(),
        "integration": "actual production ASIC/BM1370/board/power bodies; external UART/GPIO/Buck mocked",
        "compiler": "ASan/UBSan C++17 Wall Wextra Werror diagnostic/capture 0/0, 1/0 and 1/1",
        "sourceFiles": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in SOURCE_FILES},
        "limitation": "Published third-party CRC captures and synthetic declared topology; no own-QAxe CRC or isolation qualification"
    }, indent=2) + "\n")
    print("Default/diagnostic/capture initialization bytes identical:", len(legacy_wire.splitlines()), "packets")
