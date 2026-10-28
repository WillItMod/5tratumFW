# Experimental 480×320 display profiles and QAxe references

The fixed compile-time profiles below were the private 480×320 experiments used
before QAxe Beta7. **Public QAxe Beta8 carries those panel settings in one firmware as a
saved runtime selector:** see [Screen selection](display-settings.md). Its default
remains 320×170; both 3.5-inch choices remain experimental rather than physically
qualified by a compile or a guessed PCB revision. **Earlier OctAxe Beta5 retains
only 320×170.** The separate [public experimental OctAxe Beta6](octaxe-480-display.md)
uses 480×320 (153,600 pixels) as a working assumption and adds saved RGB/BGR
choices without claiming physical panel or vendor-layout compatibility.
Public helpers still reject a fixed experimental compile-time default; the
unified QAxe image is built with the standard default and verified runtime support.

| Setting | Standard | QAXE_480X320_RGB | QAXE_480X320_BGR |
| --- | --- | --- | --- |
| Viewport | 320×170 | 480×320 | 480×320 |
| Bus clock | 6.528 MHz | 12.288 MHz | 12 MHz |
| Draw buffer | 13,600 pixels | 25,600 pixels | 25,600 pixels |
| Panel colour order | RGB | RGB | BGR |
| Panel gap | 0, 35 | 0, 0 | 0, 0 |
| Mirroring in current source | Original one-axis flip | Both axes follow flip | Both axes follow flip |
| Public OTA/package admission | Existing model policy | Rejected | Rejected |

The BGR reference originally left both mirror axes off. Current source permits
180-degree rotation on both larger profiles. The package restriction in this
table concerns a fixed compile-time experimental QAxe default, rather than the
standard-default firmware with saved runtime choices. Public OctAxe Beta6 uses
that standard-default/runtime-choice approach; these QAxe references do not
qualify an OctAxe screen.

All profiles preserve the existing 8-bit Intel 8080 bus: data GPIOs
39/40/41/42/45/46/47/48, RD9, power15, WR8, CS6, DC7, reset5 and backlight38;
buttons remain 14/0. They reuse ESP-IDF's ST7789 command-compatible panel API.
This does not verify that an actual larger panel's IC is ST7789, and there is no
automatic LCD ID detection. The inherited board-version constant cannot identify
the display.

The public RGB profile reference is brunneis, GPL-3.0, pinned commit
`451d95af7953f6f5b1c94b7627db81d4bcbadcd9`, with unchanged stock partitions:
[driver](https://github.com/brunneis/nerdqaxeplus2-3.5-inches/blob/451d95af7953f6f5b1c94b7627db81d4bcbadcd9/main/displays/displayDriver.cpp),
[settings](https://github.com/brunneis/nerdqaxeplus2-3.5-inches/blob/451d95af7953f6f5b1c94b7627db81d4bcbadcd9/main/displays/displayDriver.h).
AguiMr's GPL-3.0 implementation, which credits brunneis, corroborates the guarded
RGB display path at `22a791a1545c42a16ad3ef65f7bc45a90ee6913d`:
[driver](https://github.com/AguiMr/nerdqaxeplus2-ethernet/blob/22a791a1545c42a16ad3ef65f7bc45a90ee6913d/main/displays/displayDriver.cpp).
Its unrelated Ethernet and partition changes are not imported.

The BGR settings reference is XTVDDICT's GPL-3.0 YYSLUPING-specific port, pinned
commit `661273b386374bbb72cc92c94cec568178d1012f`:
[driver](https://github.com/XTVDDICT/NerdQaxePlus-Rev6.1-BigScreen/blob/661273b386374bbb72cc92c94cec568178d1012f/main/displays/displayDriver.cpp).
That port also has a distinct 32 MB BOYA flash profile and changed partitions.
**None of that flash-driver or partition work is imported here.** Keeping the
stock partition table does not qualify migration from a vendor's 32 MB layout.

The seller's [replacement 3.5-inch screen](https://yysluping.com/products/the-replacement-big-screen-of-nerdqaxe-3-5inch)
advertises the size but warns about firmware/screen compatibility. It supplies
no controller ID or flash geometry that establishes a match to the owned QAxe.

## Online firmware reference check — 28 October 2026

The currently available open-source builds were checked again against the
saved profiles. AguiMr's [published dual-screen release](https://github.com/AguiMr/nerdqaxeplus2-ethernet/releases/tag/v1.1.1-alpha3-ethernet.1)
includes distinct 320×170 and 480×320 application images. Its
[large-screen settings](https://github.com/AguiMr/nerdqaxeplus2-ethernet/blob/v1.1.1-alpha3-ethernet.1/main/displays/displayDriver.h#L45-L59)
use 480×320, a 25,600-pixel draw buffer and a 12.288 MHz pixel clock.
Its current source at `556832766551fb713d8abbddd1490bb58f2b90a8`
[retains the large-panel mirror and offset correction](https://github.com/AguiMr/nerdqaxeplus2-ethernet/blob/556832766551fb713d8abbddd1490bb58f2b90a8/main/displays/displayDriver.cpp#L779-L803):
swap X/Y, mirror neither axis normally (both for 180-degree rotation), and use
zero panel gap. These values agree with our RGB profile. Brunneis's explicit
[mirror fix](https://github.com/brunneis/nerdqaxeplus2-3.5-inches/commit/451d95af7953f6f5b1c94b7627db81d4bcbadcd9)
and XTVDDICT's pinned BGR driver above remain unchanged references.

This directly addresses the reported combination of a small image towards the
upper-right, large unused margins and horizontally reversed text: the standard
profile has the wrong viewport, a 35-pixel gap and one-axis mirroring for that
panel. Size selection changes all three settings together. A 180-degree rotate
option alone does not remove a reflection. Wrong colour order is handled by
choosing the RGB or BGR profile, separately from orientation.

Only display parameters are used as reference. Other forks' Ethernet support,
flash drivers and partition tables are outside this change. Their photographs
and release binaries do not identify tadets' particular display controller;
the fitted panel still needs an actual display check. No reference binary is
substituted for the matched 5tratumFW application and web image.

## QAxe fixed-profile reference builds and restrictions

A private target compilation requires `BOARD=NERDQAXEPLUS2`, a new isolated
build directory, `FIVETRATUM_EXPERIMENTAL_DISPLAY=ON`, and exactly one of
`FIVETRATUM_DISPLAY_PROFILE=QAXE_480X320_RGB` or `QAXE_480X320_BGR`.
The actual CMake policy rejects unknown names, other boards, missing opt-in,
and any experimental selection alongside `FIVETRATUM_RELEASE_PROFILE`.
No factory/NVS image or public release asset is produced for these profiles.

The runtime selector introduced in unpublished QAxe Beta7 and published in Beta8
persists the user's selection and advertises saved/active profile metadata.
Its manifest declares the supported runtime profiles. OctAxe Beta6
has its own family identity and matching pair; build it with the
standard default, following the [OctAxe build guide](build-5tratumfw-oct.md),
rather than selecting a fixed QAxe experimental build. Known legacy application
and upstream factory updates are rejected while a larger profile is saved or
active, preventing an automatic return to the old standard-only application.
Central OS/MUX controls still require their own exact version/source admission;
profile metadata does not widen physical-board qualification or independent
ASIC assignment. Earlier **QAxe Beta6** and **OctAxe Beta5** downloads remain
standard-only images.

`test/host/test_display_profiles.py` executes the real CMake selection policy and
compiled production constexpr profiles, including negative/reconfiguration
cases. Native rendering exercises 54 pages at each viewport and all six candidate
variants. These checks establish source/layout behavior, not physical support for
an unidentified seller screen. Board, regulator, clock/voltage settings, existing
non-display settings, NVS layout and partition geometry remain unchanged. The
runtime selector adds only the saved display-profile value within the existing
settings store. The separate [OctAxe Beta6 validation](validation-oct-beta6.md)
records its completed build and software checks. Those checks and the earlier
QAxe evidence do not establish a physical Beta6 installation or panel test.
