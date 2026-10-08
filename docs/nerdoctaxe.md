# NerdOCTAXE-Gamma candidate

The first OctAxe port is a **source and build candidate**, with paired version `5tratumFW-oct-0.1.0-beta.1` and intended prerelease tag `octaxe-v0.1.0-beta.1`. This guide does not announce a published or physically qualified release.

Its exact target is **NERDOCTAXEGAMMA**, reported model **NerdOCTAXE-γ**, **eight BM1370 ASICs**, and an ESP32-S3 controller. The application asset is `esp-miner-NerdOCTAXE-Gamma.bin`; its `www.bin` belongs to the same OctAxe package. The ASCII asset/mining-agent name and the existing UTF-8 API model name describe the same selected software target.

The test unit's physical PCB revision, regulator population and installed flash/partition geometry remain unqualified. Neither the model name nor the compiled eight-chip profile identifies a physical revision. Other OctAxe models/revisions, NERDOCTAXEPLUS/BM1368, QAxe and Gamma have no compatibility claim from this port. Existing Gamma Beta 5 and QAxe Beta 3 packages remain separate, unchanged releases.

- [Choose the correct firmware family](firmware-selection.md)
- [OctAxe source, pinned build and package](../firmware/nerdqaxe/docs/build-5tratumfw-oct.md)
- [Migration requirements, settings preservation and recovery](../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md)
- [Beta 1 candidate validation and physical qualification gates](../firmware/nerdqaxe/docs/validation-oct-beta1.md)
- [Next models: OctAxe, GT, then Gamma Duo 650](next-models.md)

The port reuses the unified web navigation, named pool/tuning slots, pool and power schedules, both fans and the branded display. Eight-chip counter pages and the recurring large 5tratumFW logo require separate physical-screen confirmation. Missing chip temperatures remain unavailable; shared board and regulator readings must not become eight invented chip readings.

Native A/B connections use one stable parent identity, separate sessions and per-session MUX coin/job context. They share the ASIC chain and its existing job selector; `independentWorkAssignment` stays false. Current MUX/OS remote management allowlists are separately qualified and must not be assumed to admit OctAxe merely because native session advertisements are implemented.
