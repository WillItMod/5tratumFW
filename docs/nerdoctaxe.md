# NerdOCTAXE-Gamma BETA — scoped compatibility

The first qualified OctAxe BETA pair is **`5tratumFW-oct-0.1.0-beta.3`**, tag `octaxe-v0.1.0-beta.3`. It is built from source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`; family CI and offline checks passed. **The exact pair passed a supervised update and short retention/mining check on the owned PCB 2.2 unit on 8 October 2026.** Read [Beta3 validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md) before installation. The earlier Beta1/Beta2 bench applies to one owned PCB 2.2 unit, not every revision or the new pair.

Its exact target is **NERDOCTAXEGAMMA**, reported model **NerdOCTAXE-γ**, **eight BM1370 ASICs**, and an ESP32-S3 controller. The application asset is `esp-miner-NerdOCTAXE-Gamma.bin`; its `www.bin` belongs to the same OctAxe package. The ASCII asset/mining-agent name and the existing UTF-8 API model name describe the same selected software target.

The owner reported PCB revision 2.2. Its actual 16 MiB flash, 3 MiB WWW and two 4 MiB OTA slots were read and verified before installation, with a private full-flash recovery backup. Regulator strap, stock settings and the pinned revision 2.2 hardware source were reviewed; the regulator package marking was not physically read. Neither the model name nor the compiled eight-chip profile identifies another physical assembly. Other OctAxe models/revisions, NERDOCTAXEPLUS/BM1368, QAxe and Gamma have no compatibility claim from this port. The current qualified [Gamma Beta7](validation-beta7.md) and [QAxe Beta5](../firmware/nerdqaxe/docs/validation-qa-beta5.md) BETA pairs are separate families; earlier releases retain their own historical records.

- [Choose the correct firmware family](firmware-selection.md)
- [OctAxe source, pinned build and package](../firmware/nerdqaxe/docs/build-5tratumfw-oct.md)
- [Migration requirements, settings preservation and recovery](../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md)
- [Current Beta3 installed-pair validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md)
- [Earlier Beta 2 installed-pair validation and connection-options fix](../firmware/nerdqaxe/docs/validation-oct-beta2.md)
- [Earlier Beta 1 scoped hardware bench and remaining gates](../firmware/nerdqaxe/docs/validation-oct-beta1.md)
- [Next models: OctAxe, GT, then Gamma Duo 650](next-models.md)

Pool routing places the Primary/A and Secondary/B editors above ten named slots. Save the connection first, then use **Save to slot** to capture that saved connection with a name. Saved slots offer **Apply to Primary** and **Apply to Secondary**; selecting a slot alone changes no pool. Strict acknowledgment and profile/settings readback checks keep an uncertain write unconfirmed, without automatic replay.

The port retains the unified web navigation, named tuning slots, pool and power schedules, both fans and the branded display. Eight-chip counter pages and the recurring large 5tratumFW logo require separate physical-screen confirmation. Missing chip temperatures remain unavailable; shared board and regulator readings must not become eight invented chip readings.

Native A/B connections use one stable parent identity, separate sessions and per-session MUX coin/job context. They share the ASIC chain and its existing job selector; `independentWorkAssignment` stays false. Current MUX/OS remote management allowlists are separately qualified and must not be assumed to admit OctAxe merely because native session advertisements are implemented.
