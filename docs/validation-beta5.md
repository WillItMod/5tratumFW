# Gamma Beta 5 validation

Paired version: **5tratumFW-0.1.0-beta.5**. Gamma PCB601/602 only; the explicit stored-identity guard is unchanged.

This version replaces the separate “5” emblem and typed brand name with the approved complete 5tratumFW logo in the web header, loading screen and favicon. The same approved PNG is used by QAxe: SHA256 `8bfb9a51b658a73e3a71ec2e87c1e7ff8a669831a902841b4052ed231c4d35a7`. The old Gamma emblem asset is removed, reducing image payload by 1,722 bytes. Desktop masthead spacing is adjusted for the full artwork; compact layout retains its controls row.

No Gamma hardware initialization, clocks, voltage, cooling, protections, NVS keys, partition layout, pools, profiles or scheduler behavior changes. This is a web branding change and a paired app/web version update. No new hardware compatibility is added.

## Qualification

The final release manifest records exact build source and app/web/source hashes. The pinned build runs the full frontend suite, production Gamma host regressions, ESP-IDF 5.5.3 application/WWW build, 27 native OLED fixture views and byte-identical SPIFFS reconstruction. Browser layout checks cover desktop and compact widths. Target builds and simulated fixtures do not establish physical operation.

The separate release validation asset records final results and any new physical OTA observations. [Beta 4 installed-device evidence](validation-beta4.md) remains historical evidence for that earlier pair. Gamma602 remains unflashed unless this version's own validation records otherwise. No 650 DUO, GT800 or GT801 support is added by the shared branding.

Install only the matched Gamma `esp-miner.bin` and `www.bin` from one package. Preserve the existing operating point and settings. QAxe's shared `www.bin` filename is a separate, incompatible image family.
