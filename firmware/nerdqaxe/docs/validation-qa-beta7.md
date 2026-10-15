# QAxe Beta7 display-selector validation

Version: `5tratumFW-qa-0.1.0-beta.7`; intended tag `qaxe-v0.1.0-beta.7`.
Build/source receipt, final test results and installed-device observations are
pending. Beta6 assets and their dated validation remain unchanged.

This change adds a persisted, explicit selector for the standard 320×170 panel
and two 480×320 panel profiles. Existing settings keep the standard default.
Active geometry is latched on boot; saving a new profile does not reinitialize
the live panel. Invalid settings and unsupported board/profile combinations
must be rejected before any configuration writes. Runtime metadata is separate
from exported durable settings. Panel inversion is applied with the old default
visual behavior retained.

The profile changes only the display viewport, draw buffer, pixel clock, colour
order, offset and orientation. Board guards, GPIO wiring, ASIC/regulator drivers,
voltage/frequency, fans, pools, slots, schedules and flash geometry are preserved.
The larger profiles remain experimental; software rendering and target builds
do not establish compatibility with an unidentified physical 3.5-inch panel.

Before packaging, verify persistence across boot, requested versus active state,
stock rendering invariance, all larger layout/candidate views, malformed/atomic
settings rejection, display-only save preservation, downgrade rejection before
ASIC shutdown, manifest/compiler agreement and normal paired-image checks.
Record the exact compiled source and package hashes here after a clean build.
