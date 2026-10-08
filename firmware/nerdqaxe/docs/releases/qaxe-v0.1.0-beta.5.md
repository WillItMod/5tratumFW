# QAxe Beta5 — BETA release notes

`5tratumFW-qa-0.1.0-beta.5` · `qaxe-v0.1.0-beta.5` · source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`.

Pool routing puts the Primary and Secondary editors above ten named slots. Save pool settings and Save to slot are separate actions. Each configured slot offers Apply to Primary and Apply to Secondary. Selecting a slot does not apply it. Invalid or ambiguous replies stay unconfirmed, without automatic write retry.

Compatibility: NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 / ESP32-S3 only. The owned test unit's PCB revision remains unidentified.

Install `esp-miner-NerdQAxe++.bin` and its matching `www.bin` together. Existing clocks, voltage, cooling, network, credentials, profiles and schedules retain their established preservation behavior. Verify the actual retained state after upload and keep a private recovery backup. No factory/NVS image or automatic tuning is included.

Full family CI and offline package checks passed. The exact pair passed a supervised OTA/94-asset/settings-retention check on 8 October 2026, with four fresh ASIC counters and accepted-share progress of 2. Both native A/B feeds were fresh; the retained mode was Dual pool at 50/50. This short bench does not qualify a thermal fault, new power-schedule boundary, sustained soak or physical display readability. [QAxe Beta5 validation](https://github.com/WillItMod/5tratumFW/blob/main/firmware/nerdqaxe/docs/validation-qa-beta5.md) · [Model guide](https://github.com/WillItMod/5tratumFW/blob/main/docs/nerdqaxe.md).

Native A/B sessions share the chain; neither this layout nor separate routes establishes physical ASIC ownership. Package hardware-test flags describe the pre-bench audit and remain unchanged; dated validation records add any later bench results.
