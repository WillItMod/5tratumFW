#!/usr/bin/env python3
"""Compile real display candidate methods against a no-hardware LVGL fixture."""
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
sources = {
    "main/displays/ui.cpp": [
        "void UI::showCandidateOverlay(", "void UI::updateCandidateOverlayAnimation(",
        "void UI::hideImageOverlay(", "void UI::showErrorOverlay(", "void UI::hideErrorOverlay(",
    ],
    "main/displays/displayDriver.cpp": [
        "void DisplayDriver::showFoundBlockOverlay(", "void DisplayDriver::hideFoundBlockOverlay(",
        "void DisplayDriver::showError(", "void DisplayDriver::hideError(",
        "void DisplayDriver::startShutdownCountdown(", "void DisplayDriver::hideShutdownCountdown(",
        "uint32_t DisplayDriver::handleLvglTick(",
        "void DisplayDriver::updateOverlayPriority(",
    ],
}
methods, report = [], []
for relative, signatures in sources.items():
    path = root / relative
    text = path.read_text()
    methods.extend(body(text, signature) for signature in signatures)
    report.append({"source": relative, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                   "productionMethods": len(signatures)})
state = body((root / "main/displays/displayDriver.cpp").read_text(), "bool DisplayDriver::enterState(")
priority_cases = []
for name, following in (("ShowQR", "Identify"), ("Identify", "PowerOff")):
    start = state.index(f"    case UiState::{name}:")
    end = state.index(f"    case UiState::{following}:", start)
    priority_cases.append(state[start:end])
start = state.index("    case UiState::PowerOff:")
end = state.index("    }\n    updateOverlayPriority();", start)
priority_cases.append(state[start:end])
methods.append("void DisplayDriver::fixtureEnterPriorityState(UiState state, int64_t now) {\n"
               "    (void)now; m_state = state;\n    switch (state) {\n" + "".join(priority_cases) +
               "    default: break;\n    }\n    updateOverlayPriority();\n}\n")
report.append({"source": "main/displays/displayDriver.cpp", "productionPriorityCases": ["ShowQR", "Identify", "PowerOff"]})
buttons = body((root / "main/displays/displayDriver.cpp").read_text(), "void DisplayDriver::processButtons(")
buttons = buttons[:buttons.index("    // --- Shutdown countdown handling ---")]
buttons = buttons.replace("void DisplayDriver::processButtons(", "void DisplayDriver::fixtureProcessOverlayButtons(")
buttons += "    (void)bothPressed; (void)btn1Press; (void)btn2Press; (void)btnBothLongPress;\n}\n"
methods.append(buttons)
report.append({"source": "main/displays/displayDriver.cpp", "productionOverlayButtonHandling": True})
(output / "candidate_methods.inc").write_text("\n".join(methods))
(output / "candidate-source.json").write_text(json.dumps(report, indent=2) + "\n")
