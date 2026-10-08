#!/usr/bin/env python3
"""Compile selected real display method bodies behind cached-data fixtures only."""
from pathlib import Path
import hashlib
import json
import sys


def body(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


root, output = map(Path, sys.argv[1:])
driver_path = root / "main/displays/displayDriver.cpp"
manager_path = root / "main/stratum/stratum_manager.h"
driver = driver_path.read_text()
methods = [body(driver, signature) for signature in (
    "void DisplayDriver::updateAsicReadings()",
    "void DisplayDriver::updateThermalReadings(int64_t nowUs)",
    "void DisplayDriver::updateCoinContext(const StratumManager::MuxDisplayState &view)",
    "void DisplayDriver::updateGlobalMiningStats(const StratumManager::MuxDisplayState &view)",
)]
state = body(driver, "void DisplayDriver::updateState(")
start = state.index("    case UiState::AsicScreen:")
end = state.index("    case UiState::SettingsScreen:", start)
case = state[start:end]
methods.append("void DisplayDriver::fixtureAsicButton(int64_t now, bool btn1Press, bool btn2Press) {\n"
               "    switch (UiState::AsicScreen) {\n" + case + "    default: break;\n    }\n}\n")
snapshot = body(manager_path.read_text(), "    struct MuxDisplayState")
(output / "display_mux_snapshot.inc").write_text("class StratumManager { public:\n" + snapshot + ";\n};\n")
(output / "display_driver_methods.inc").write_text("\n".join(methods))
(output / "display-driver-source.json").write_text(json.dumps({
    "source": "main/displays/displayDriver.cpp",
    "sha256": hashlib.sha256(driver_path.read_bytes()).hexdigest(),
    "productionMethods": 4,
    "productionAsicButtonCase": True,
    "productionMuxSnapshot": True,
    "fixtureOnly": True,
}, indent=2) + "\n")
