# QAxe legacy updater source fix — unreleased

Checked on 8 October 2026. This record covers the browser/source correction after a report of **Filename does not match this device** with **Expected: Loading device model…**. The immutable `qaxe-v0.1.0-beta.1` release assets do not contain this fix.

The update page now obtains model/version from the shared v1 `/api/system/info` endpoint. It blocks application upload until identity is available, distinguishes a failed identity request from a filename mismatch, and provides a retry. It retains the exact `esp-miner-NerdQAxe++.bin` filename for the reported QAxe++ model.

When `/api/v2/identify` returns 404 or 501, authentication can read the explicit OTP flag from v1. Missing/malformed authentication information and authentication/network errors remain failures; a missing endpoint does not imply OTP is disabled. Other settings pages still use their existing v2 contracts.

## Checks

- All 55 Angular/Karma browser unit tests passed, including loading/error/retry, exact model filenames, v1 identity projection, both legacy OTP states, absent OTP and a rejected authentication request.
- The production Angular build passed. Initial JavaScript/CSS payload: 3.41 MB raw, 469.66 kB estimated transfer size, within its existing 5 MB initial budget. Existing third-party CommonJS warnings remain.
- Thirteen checks passed against the compiled production interface in headless Chromium with a simulated `v1.0.37.3-LTS` QAxe++ and all v2 endpoints returning 404. Matching files become available after identity arrives; another model's filename remains rejected. A failed v1 request displays the identity error, a successful retry clears it, and the interface fits a 390-pixel viewport. No OTA requests or physical-device writes were made during these browser checks.
- An independent source review found no material correctness or authentication issue.
- [Linux QAxe CI](https://github.com/WillItMod/5tratumFW/actions/runs/37754231560) passed for source commit `f65c4379aef383da3960043f8d300ce3570d907e`, including the pinned target build, host/browser checks and candidate OTA/source packaging. The duplicate PR build also passed. Both Gamma CI jobs passed with Gamma source unchanged. CI artifacts are unreleased candidates, not replacements for the published Beta 1 assets.

## Hardware and release boundary

The QAxe++ driver has one `NERDQAXEPLUS2` target and no separate PCB 6.0/6.1 compile profile. This source review is not physical testing of either revision. The existing test unit's PCB revision remains unidentified; the inherited rev7 regulator path is also unqualified.

No application/WWW OTA pair was flashed or published as a release for this source fix. No operating settings, hardware initialization, drivers, partition table, NVS or Gamma source were changed. Follow a future release's own image hashes and validation before treating the source correction as an installed-device result.
