# Experimental QAxe 480×320 display profiles

The fixed compile-time profiles below were the private 480×320 experiments used
before QAxe Beta7. **QAxe Beta7 adds these panel settings to one firmware as a
saved runtime selector:** see [Screen selection](display-settings.md). Its default
remains 320×170; both 3.5-inch choices remain experimental rather than physically
qualified by a compile or a guessed PCB revision. OctAxe retains only 320×170.
Public helpers still reject a fixed experimental compile-time default; the
unified QAxe image is built with the standard default and verified runtime support.

| Setting | Standard | QAXE_480X320_RGB | QAXE_480X320_BGR |
| --- | --- | --- | --- |
| Viewport | 320×170 | 480×320 | 480×320 |
| Bus clock | 6.528 MHz | 12.288 MHz | 12 MHz |
| Draw buffer | 13,600 pixels | 25,600 pixels | 25,600 pixels |
| Panel colour order | RGB | RGB | BGR |
| Panel gap | 0, 35 | 0, 0 | 0, 0 |
| Mirroring | Original one-axis flip | Both axes follow flip | Neither axis |
| Public OTA/package admission | Existing model policy | Rejected | Rejected |

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

## Compile selection and restrictions

A private target compilation requires `BOARD=NERDQAXEPLUS2`, a new isolated
build directory, `FIVETRATUM_EXPERIMENTAL_DISPLAY=ON`, and exactly one of
`FIVETRATUM_DISPLAY_PROFILE=QAXE_480X320_RGB` or `QAXE_480X320_BGR`.
The actual CMake policy rejects unknown names, other boards, missing opt-in,
and any experimental selection alongside `FIVETRATUM_RELEASE_PROFILE`.
No factory/NVS image or public release asset is produced for these profiles.

Beta7 persists the user's selection and advertises saved/active profile metadata.
Its manifest declares the supported runtime profiles. Known legacy application
and upstream factory updates are rejected while a larger profile is saved or
active, preventing an automatic return to the old standard-only application.
Central OS/MUX controls still require their own exact version/source admission;
profile metadata does not widen physical-board qualification or independent
ASIC assignment. Earlier Beta6 and its downloads remain standard-only images.

`test/host/test_display_profiles.py` executes the real CMake selection policy and
compiled production constexpr profiles, including negative/reconfiguration
cases. Native rendering exercises 54 pages at each viewport and all six candidate
variants. These checks establish source/layout behavior, not physical support for
an unidentified seller screen. Board, regulator, clock/voltage settings, NVS and
partition geometry are outside this display change.
