# Firmware BETA release index

Choose by actual model/PCB and use one complete matching app/WWW package. Family images are not interchangeable. See [firmware selection](../firmware-selection.md).

| Family | Version and tag | Current evidence |
| --- | --- | --- |
| Gamma PCB 601/602 / BM1370×1 | `5tratumFW-0.1.0-beta.8` · `v0.1.0-beta.8` | Qualified schedule JSON-header BETA pair. Full build/offline checks, exact-source CI and separate 601/602 paired checks passed; installed strict OS31 inspection completed on both models. [Notes](v0.1.0-beta.8.md) · [Validation](../validation-beta8.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `5tratumFW-qa-0.1.0-beta.5` · `qaxe-v0.1.0-beta.5` | Qualified published BETA pair; test PCB revision unknown. [Validation](../../firmware/nerdqaxe/docs/validation-qa-beta5.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `5tratumFW-oct-0.1.0-beta.3` · `octaxe-v0.1.0-beta.3` | Qualified published BETA pair; owned PCB 2.2 unit only. [Validation](../../firmware/nerdqaxe/docs/validation-oct-beta3.md) |

Gamma Beta8 source is `eff5e633f372bb46a86803580c453645600f5fcd`; QAxe Beta5 and OctAxe Beta3 retain `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`. The Gamma-only API fix does not rebuild their pairs.

Earlier Gamma [Beta7](../validation-beta7.md), [Beta6](../validation-beta6.md), [Beta5](../validation-beta5.md), [Beta4](../validation-beta4.md), [Beta3](../validation-beta3.md) and [Beta1](../validation.md) records remain historical evidence for their own images. Earlier QAxe/OctAxe records remain linked from their current guides. Publication does not qualify other PCB revisions; native A/B sessions do not establish independent physical-chip ownership.
