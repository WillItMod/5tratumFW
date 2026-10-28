# Gamma W5500 Ethernet — public experimental BETA

This feature is published in **Gamma Beta11 — EXPERIMENTAL BETA**, based on ESP-Miner v2.15.4. It is **off by default** and has not been tested on a physical LAN adapter. Beta11 has not been installed on a miner; its qualification is software/build evidence only. The previous physically checked [Gamma Beta10 pair](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) remains available. This option applies to Bitaxe Gamma **PCB 601/602 with BM1370**; it does not add Ethernet support to the separate NerdQAxe++ or NerdOctAxe Gamma images.

## Enable Ethernet

Use the matching Beta11 `esp-miner.bin` and `www.bin` from the [public Gamma release](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.11). Follow the [Gamma installation guide](installation.md); keep a recovery route and the unit's saved settings. Installing a Gamma image on a QAxe or OctAxe is unsupported.

1. Install the matching Beta11 pair over Wi-Fi with the adapter disconnected. Open the miner's 5tratumFW interface.
2. Open **Network**, select **Enable wired Ethernet (W5500)**, then choose **Save network settings**. Saving alone does not start or stop the adapter.
3. Power off the miner. Connect the compatible W5500 SPI adapter using the wiring below, after confirming its supply voltage from its own documentation, then power on. This boot applies the saved setting and reserves the accessory pins before BAP can start. For later setting changes with unchanged wiring, **Restart device** also applies the saved choice.
4. After restart, check **Wired Ethernet · Experimental** for the Ethernet address and **Active connection: Ethernet**. The router may assign a different address from the miner's Wi-Fi address. Reopen the miner at the Ethernet address if the old browser connection closes.

The page distinguishes **Saved** from **At startup** so an unapplied change remains visible after reloading. Status is refreshed while the page is open. A missing module reports **not detected** and keeps Wi-Fi available; enabling the setting does not create an Ethernet connection by itself.

Ethernet becomes the preferred connection after it obtains a usable address. The Wi-Fi association is then disconnected to expose one active uplink while retaining its credentials for fallback. Cable loss starts Wi-Fi recovery and makes the configuration access point available while disconnected. An interface or address change reconnects the current pool session without selecting another pool, profile or protocol. The miner's existing API identity is retained for 5tratumOS discovery.

To disable Ethernet, clear the checkbox and save, then power off, disconnect the module and power on. This avoids starting BAP on pins still connected to the module. Keep the module disconnected before using the same accessory pins for BAP or installing older firmware that does not reserve them.

## Install through 5tratumOS

With **5tratumOS 0.8.48 or later**, open **Fleet → Devices** in Orbit, or **5tratumFW devices** in Classic. On the correct Gamma 601/602, select **Firmware update → BETA → Check for updates**, confirm **Bitaxe Gamma 601/602 Beta11 — experimental**, then **Install firmware → Confirm install**. Keep it powered until **Firmware installation verified**. This installs both matching images; it does not enable Ethernet. Keep the adapter disconnected and use Wi-Fi for installation and verification, then follow the enable steps above. Initial conversion from stock firmware uses the [manual Gamma guide](installation.md).

Future compatible releases are picked up from the BETA channel without another OS update for each firmware version. See [complete one-click instructions](../DOWNLOADS.md#update-through-5tratumos). Ethernet-only application/WWW updates still require physical qualification.

## Wiring

| W5500 signal | Gamma GPIO |
| --- | --- |
| MOSI | 40 |
| MISO | 39 |
| SCLK | 41 |
| CS / SCS | 42 |
| GND | Common ground |

SPI uses mode 0 at 20 MHz. This implementation polls the module and has no interrupt or reset GPIO. The W5500 chip uses a **3.3 V supply**; some complete modules have a regulator and a different permitted input supply. Use the exact module's specified supply and compatible 3.3 V logic. A generic W5500 name is insufficient to assume a 5 V input is safe.

GPIO39/40 overlap the default BAP accessory connection. The saved opt-in reserves the affected accessory pins for that boot, including self-test and missing-adapter paths. BAP cannot share those pins with the adapter. Known conflicts with configured display/I2C or active board-control pins reject Ethernet startup; the firmware does not rewrite those pin settings.

## Implementation and qualification

The new driver is original code in this repository, written from WIZnet register/protocol specifications and public ESP-IDF APIs. No implementation from CryptoIceMLH/ESP-Miner-LAN was incorporated. Its README/release information was used only as a hardware/wiring reference. The existing firmware and this new component retain GPL licensing and the repository's notices. See the [component description and source references](../components/gamma_ethernet/README.md).

The flag is a separate durable setting. Toggling Ethernet alone leaves saved Wi-Fi, pools, clocks, voltage, cooling, slots and schedules untouched. The driver uses the host network stack for DHCP, DNS, discovery and mining traffic. Host tests and target compilation cannot prove module wiring, physical packet framing or reliable Ethernet-only OTA.

To qualify this experimental release on the requester's module, record:

- Exact miner PCB and adapter model, supply and wiring; readable screen and normal fan operation.
- Disabled/default behavior, enable/save/restart persistence and module-absent Wi-Fi recovery.
- Link, DHCP address, DNS, web interface, mDNS discovery and accepted mining shares over Ethernet.
- Cable removal/reconnection, Wi-Fi fallback and return to Ethernet; no repeated pool fallback or duplicate inventory entries.
- DHCP address changes, sustained RX/TX traffic, memory stability and restart recovery.
- Both matching application and WWW updates while Ethernet is the only usable connection, followed by complete readback and settings verification.

The adapter is with the person who requested LAN support; none is available locally. No miner has been flashed with this feature, and no physical LAN or Ethernet-only OTA result is claimed. [Validation evidence](validation-gamma-ethernet.md) separates software results from those remaining checks.
