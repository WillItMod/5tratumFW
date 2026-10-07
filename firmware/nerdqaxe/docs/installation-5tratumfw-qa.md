# QAxe paired OTA installation and recovery

## Identify before choosing

This BETA targets NerdQAxe++ (`NERDQAXEPLUS2`, BM1370 ×4). Confirm the actual board/model, ASIC type/count, existing firmware settings and partition layout. Physical PCB revisions have different operating points; the test unit's revision remains unknown. A similar product name does not identify the wiring. Gamma, QAxe+ and OctAxe images are different families.

Download the **QAxe-specific matched pair** and its `manifest.json`, `SHA256SUMS` and corresponding source. On macOS/Linux, verify with `shasum -a 256 esp-miner-NerdQAxe++.bin www.bin`, comparing both entries against the published checksums. Keep the original firmware, current settings and a private full-flash backup for recovery.

## Routine OTA upgrade

1. Confirm normal power, working network access and cooling. Record configured frequency, voltage, version rolling, fan settings, both pool connections, ten pool/tuning slots and both schedules. Masked passwords are not an exported credential backup.
2. Open **Updates**. Upload the matching QAxe `www.bin` through the web-interface uploader. Refresh after completion.
3. Upload `esp-miner-NerdQAxe++.bin` through the application uploader. Let the miner finish/restart; do not interrupt power during writes.
4. Reopen the miner and confirm application and web versions match. If cached browser assets remain, reload with cache bypass. Check the device model, retained operating point, fans, temperatures, shares and both schedules.
5. If connected to MUX, confirm the physical parent and both sessions independently. Check fresh coin/job data and actual accepted shares on the intended target for A and B.

These files are application and WWW **OTA** images. They are not a complete factory image and do not include bootloader, partition table, OTA selection or NVS. Do not overwrite another board's partition layout.

## Dual MUX setup

Use two SV1 connections to the same MUX Stratum host/port, with distinct worker names. Select **Dual pool**, retain the desired whole-chain job balance and save. Changing pool mode requires a restart; review the saved/runtime state rather than assuming the request acknowledgement proves Dual is running.

MUX's **Miners → Dual MUX mining** control is shown only for the exact qualified 5tratumFW QAxe++ version/configuration. Review lists the change, Apply preserves operating/network/fan/password/profile/schedule settings, and runtime verification follows restart. OTP-enabled or CAN-master configurations are not admitted by this narrow MUX control.

A and B are network sessions. Their independent targets and counters do not prove independent physical-chip assignment. The miner's Pool routing page retains direct-pool controls and ten named slots; profile Apply targets A or B in Dual mode.

## Physical screen and recurring logo

Startup shows scrolling status events. While mining, the large 5tratumFW logo slide appears for three seconds every two minutes on the mining overview, after at least thirty seconds without a button press and while telemetry is fresh. Shutdown, thermal, enrollment and animation screens take priority.

To see the recurring slide, open **Miner controls → Display**, disable **Automatic screen off**, save, and leave the physical screen on the mining overview for at least two minutes. With automatic screen off enabled, the display normally sleeps before the slide is due; updates preserve your saved choice.

On the physical screen, **P1 means session A** and **P2 means session B** in the web interface and MUX. QAxe board and regulator temperatures are shared measurements displayed once; they are not individual chip temperatures.

## USB bootloader recovery

Use a known data-capable USB cable. macOS exposes serial devices as `/dev/cu.*`; Windows uses COM ports. A blank screen in bootloader mode is expected, but first verify a serial device appears and identifies the correct ESP32-S3.

1. Release both buttons and disconnect **both main power and USB**. Wait ten seconds.
2. Leave main power disconnected. Hold the button explicitly labelled **BOOT** while reconnecting USB only; release BOOT after two seconds. Do not guess that an unlabelled KEY button is reset.
3. List the serial ports. Run `python -m esptool --chip esp32s3 --port SERIAL_PORT chip-id` with the actual detected port; stop if the identity or device selection is uncertain.
4. Read and privately verify a full backup before any write. The QAxe reference layout is 16MiB, but use the detected layout and existing recovery record, not a guessed offset.
5. Restore only the exact verified full backup for **that same unit**, or use a complete manufacturer recovery package matching its board/partition layout. The OTA pair alone cannot reconstruct an erased device.
6. Release BOOT, disconnect USB and normal power for ten seconds, then reconnect normal power only. Keep USB unplugged while checking the original boot screen and network address.

This project does not publish factory/NVS images or an erase-flash command. A browser flasher is not included in this BETA; do not choose an upstream web-flasher model solely because its name resembles the device.
