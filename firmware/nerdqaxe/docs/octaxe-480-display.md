# OctAxe Beta6 — public experimental 480×320 screen support

**OctAxe Beta6 is a public experimental BETA, with the standard 320×170 default
and explicit 480×320 RGB/BGR choices.** [Release notes and downloads](releases/octaxe-v0.1.0-beta.6.md)
and the [dated validation](validation-oct-beta6.md) record the exact pair and
completed software checks. Neither its standard screen nor its larger profiles
have a new physical Beta6 bench. Earlier Beta5 remains a standard-only release
with its own separate owned-device evidence.

The accepted working assumption for the reported larger screen is a landscape
**480×320 viewport: 480 × 320 = 153,600 pixels**. That pixel count agrees with
the proposed resolution; it does not uniquely determine the dimensions,
orientation, display controller, colour order, bus timing or wiring. It also
provides no evidence about the miner's PCB, flash chip or partition layout.

## Reference evidence and its limits

YYSLUPING's [OctAxe product description](https://yysluping.com/products/bitaxe-nerdoctaxe-up-to-12th-s-btc-miner-lucky-miner-with-power-supply)
advertises an eight-BM1370 miner and describes its 3.5-inch screen firmware as
a vendor customisation. The page does not identify the panel controller or
establish its flash/partition compatibility with this source target.

The vendor's [vastbtc v1.1.0 firmware notes](https://github.com/yysluping/vastbtc/releases/tag/v1.1.0)
label the **B++** package as 480×320 and the **S++** package as 320×170. This is
evidence for those named screen packages. The notes do not explicitly map B++
to the particular OctAxe assembly or prove that its hardware uses our bus,
controller or flash layout. Together these sources support a test assumption,
not a qualified OctAxe panel identity. Their binaries are not substituted for
5tratumFW images.

The [existing open-source QAxe display references](experimental-480-display.md)
provide the RGB/BGR viewport, offset and mirror settings reused for this
experiment. No vendor flash driver, changed partition table, Ethernet support
or tuning values are imported.

## Installation and screen selection

Use only the matched OctAxe **`5tratumFW-oct-0.1.0-beta.6`** pair:
[**`esp-miner-NerdOCTAXE-Gamma.bin`**](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/esp-miner-NerdOCTAXE-Gamma.bin)
and [**`www.bin`**](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/www.bin),
verified against this release's [**`SHA256SUMS`**](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/SHA256SUMS).
QAxe files and vendor merged images are different targets. Beta6 retains the standard 320×170 default,
the existing board guard, bus pins, regulator/fan behaviour and flash layout.
An absent saved screen choice keeps Standard; a display-only edit does not
change clock, voltage, cooling, network/pools, slots or schedules.

Install the verified application first using the supported OTA path, wait for
its restart, then install the matching WWW and refresh. Follow the [model,
layout and recovery requirements](installation-5tratumfw-oct.md); these files
are OTA images, not a factory or NVS image. Then:

1. Open **Miner controls → Display → Screen** and choose **3.5-inch RGB**.
   Keep **Standard** for the original 320×170 screen.
2. Click **Save this section**.
3. Click **Restart device**. Refresh after the miner returns and confirm the
   active screen matches the saved choice.
4. Check the whole viewport, position and readable unmirrored text. The large
   profiles use zero panel offset and remove the standard profile's one-axis
   reflection. If colours are wrong, choose **3.5-inch BGR**, save and restart
   again. **Rotate miner screen** handles a separate 180-degree mounting
   orientation; save that change and restart as well.
5. Before installing older standard-only firmware, select **Standard**, save,
   restart and refresh to confirm Standard is active. Then use that older
   release's matching application/web pair.

Returning to Standard requires an explicit save and restart. Known older OctAxe
applications without this selector, including Beta5, must not replace Beta6
while a larger profile remains saved or active. Beta6's update
guard requires a compatible paired application or that explicit return to
Standard; refreshing a page does not reset the saved profile.

## Qualification boundary

No physical OctAxe larger panel is available in the local bench. Full-screen
position, reflection, colours, rotation, startup, refresh and saved-choice
persistence still require checking on the actual panel. The owned PCB 2.2
[Beta5 observations](../../../docs/validation-aligned-20261028.md) apply only to
that earlier standard-screen pair. They do not qualify Beta6 or another board,
panel controller or vendor flash layout. Beta6 retains its own exact
source/version/checksum receipts; no earlier binary is relabelled.

The established target remains **NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370×8**.
This screen experiment does not expand physical-board support or enable
independent per-chip routing. Native A/B sessions continue to share the chain.
