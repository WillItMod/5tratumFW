# OctAxe Beta6 — local experimental 480×320 screen candidate

**Beta6 is a local test candidate. Public OctAxe Beta5 remains unchanged and
supports only the standard 320×170 screen.** No Beta6 build result, physical
screen check or public release is established by this document.

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
provide the RGB/BGR viewport, offset and mirror settings reused for this local
experiment. No vendor flash driver, changed partition table, Ethernet support
or tuning values are imported.

## Candidate operation

Use only the matched OctAxe **`5tratumFW-oct-0.1.0-beta.6`** application and web
image after their own build/package checks: **`esp-miner-NerdOCTAXE-Gamma.bin`**
and **`www.bin`** from that same candidate package. QAxe files and vendor merged
images are different targets. The candidate retains the standard 320×170 default,
the existing board guard, bus pins, regulator/fan behaviour and flash layout.
An absent saved screen choice keeps Standard; a display-only edit does not
change clock, voltage, cooling, network/pools, slots or schedules.

1. Install the verified OctAxe application first using the supported OTA path,
   wait for its restart, then install its matching WWW and refresh.
2. Open **Miner controls → Display → Screen** and choose **3.5-inch RGB** or
   **3.5-inch BGR**.
3. Click **Save this section**, then **Restart device**. Refresh after the miner
   returns and confirm the active screen matches the saved choice.
4. Check the whole viewport, position and readable unmirrored text. The large
   profiles use zero panel offset and remove the standard profile's one-axis
   reflection. Use the other RGB/BGR choice for incorrect colour order;
   **Rotate miner screen** handles a separate 180-degree mounting orientation.

Returning to Standard requires an explicit save and restart. Known older OctAxe
applications without this selector, including public Beta5, must not replace the
candidate while a larger profile remains saved or active. The candidate's update
guard requires a compatible paired application or that explicit return to
Standard; refreshing a page does not reset the saved profile.

## Qualification boundary

No physical OctAxe larger panel is available in the local bench. Full-screen
position, reflection, colours, rotation, startup, refresh and saved-choice
persistence still require checking on the actual panel. The owned PCB 2.2
[Beta5 observations](../../../docs/validation-aligned-20261028.md) apply only to
that earlier standard-screen pair. They do not qualify Beta6 or another board,
panel controller or vendor flash layout. The candidate retains its own exact
source/version/checksum receipts; no earlier binary is relabelled.

The established target remains **NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370×8**.
This screen experiment does not expand physical-board support or enable
independent per-chip routing. Native A/B sessions continue to share the chain.
