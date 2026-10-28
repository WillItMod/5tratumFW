# OctAxe Gamma Beta6 — public experimental larger-screen selector

`5tratumFW-oct-0.1.0-beta.6` · `octaxe-v0.1.0-beta.6` · compiled/source revision `5567a2d6c1f59808f197062e565fa6525a93f97b`.

**Public experimental BETA for NERDOCTAXEGAMMA / NerdOCTAXE-γ / eight BM1370s / ESP32-S3.** Beta6 adds a saved screen selector with **Standard (320×170)**, **3.5-inch RGB (480×320)** and **3.5-inch BGR (480×320)** in one OctAxe application. Standard remains the default. Larger choices change viewport size, panel offset and mirroring together, addressing the small, offset or reflected image reported on a larger display.

**480×320 = 153,600 pixels is the accepted working assumption, not a measurement of the fitted panel.** It does not identify the controller, wiring, colour order, PCB or flash layout. The vendor's B++ firmware notes support that named package's resolution but do not explicitly identify this OctAxe assembly. No vendor flash driver or partition changes are imported. [Reference evidence and limits](https://github.com/WillItMod/5tratumFW/blob/main/firmware/nerdqaxe/docs/octaxe-480-display.md).

Download this release's **[application — esp-miner-NerdOCTAXE-Gamma.bin](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/esp-miner-NerdOCTAXE-Gamma.bin)** and **[web interface — www.bin](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/www.bin)** together. Verify both against **[SHA256SUMS](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/SHA256SUMS)** and retain the manifest, web budget and corresponding GPL source archive. Use the supported application uploader **first**, wait for its restart, then install this matching WWW and refresh. Confirm both versions are Beta6. These are OTA partition images, without factory/NVS or replacement partition images. QAxe, single-chip Gamma and vendor merged firmware are different targets. Preserve the exact unit's identity, existing settings and recovery backup; follow the [OctAxe installation and layout requirements](https://github.com/WillItMod/5tratumFW/blob/main/firmware/nerdqaxe/docs/installation-5tratumfw-oct.md).

To enable and check the larger screen:

1. Open **Miner controls → Display → Screen** and select **3.5-inch RGB**. Keep **Standard** for the original 320×170 screen.
2. Click **Save this section**.
3. Click **Restart device**, wait for the miner to return, then refresh and confirm the active screen matches the saved choice.
4. Check the full-screen position and readable, unmirrored text. If colours are wrong, select **3.5-inch BGR**, save and restart again. **Rotate miner screen** handles a separate 180-degree mounting orientation; save and restart after changing it.
5. Before downgrading to Beta5 or other standard-only firmware, select **Standard**, save, restart and refresh to confirm it is active. Then install that older release's matched application/web pair. Beta6 refuses known older standard-only applications while a larger screen remains saved or active.

The existing RC1 base, saved tuning-profile schedules, clocks/voltage, both fans, protection, pools and slots remain part of the separate OctAxe family. A display-only save does not alter them. Native A/B sessions share the ASIC chain; independent per-chip routing remains unsupported and disabled. Central OS/MUX admission still requires its own exact-release update.

The pinned target build, **159 browser tests**, **29 host entry points**, **20 web-control checks**, native simulated display checks and independent package/source audit passed. **No new physical Beta6 installation was tested, including Standard.** Earlier owned-PCB-2.2 observations belong to Beta5 only. The larger panel still needs physical position, reflection, colours, rotation, startup, refresh and persistence checks. Other PCB revisions or vendor flash layouts are not qualified. The unchanged manifest records `hardwareTested: false` and `physicalLayoutVerified: false`. [Exact validation and limitations](https://github.com/WillItMod/5tratumFW/blob/main/firmware/nerdqaxe/docs/validation-oct-beta6.md).

| Image | Bytes | SHA-256 |
| --- | ---: | --- |
| `esp-miner-NerdOCTAXE-Gamma.bin` | 3,585,312 | `6a74c40c86e8232fe00de95538d44ece4b7257b1fe8dd1bd170ae352d3b7fef6` |
| `www.bin` | 3,145,728 | `11ffb541424ccaf9331211d3e7b29991a5a8028e57f9d432a09e2adb589e331c` |

All three [GitHub publication checks](https://github.com/WillItMod/5tratumFW/blob/main/firmware/nerdqaxe/docs/validation-oct-beta6.md#github-publication-checks) passed for `9aa01916`, whose production source matches the compiled pair. Publication retains the original `5567a2d6` image/source receipts.
