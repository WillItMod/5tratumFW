# Preserve the miner's settings

5tratumFW preserves the existing NVS settings during a normal application or WWW OTA update. Startup reads legacy settings into memory without rewriting their keys. It does not erase NVS when storage initialization fails. An unreadable stored board identity or an identity other than `601`/`602` stops startup before hardware initialization.

This preserves each Gamma's own operating point. The project does not apply automatic tuning or a new clock/voltage preset during installation. Do not replace your recorded values with a suggested value copied from another miner.

## Record settings before updating

Record the configured values, rather than only live measured voltage or the instantaneous frequency during a ramp:

- Frequency in **MHz** and core voltage in **mV**.
- Automatic/manual fan mode, manual and minimum fan speeds, and temperature target.
- Primary and fallback pool hosts, ports, worker identities, protocol/security options, and payout identity.
- Wi-Fi and hostname settings, display preferences, and any saved weekly schedule.

Keep Wi-Fi and pool passwords in a private record. Do not commit device exports, backups, screenshots containing credentials, or NVS dumps to this repository.

If the installed web interface has **Miner controls → Export settings**, use it to download a fresh operating-settings JSON record. It validates a Gamma `601`/`602` identity and positive configured frequency/voltage, then includes the available operating fields and their units. It excludes passwords and other NVS contents.

The export is **not a complete NVS backup** and has no automatic import/restore function. In particular, it does not include the weekly scheduler configuration, Wi-Fi credentials, pool passwords, CA certificates, SV2 authority keys, or every unknown/vendor NVS field. Record relevant settings separately. Older firmware may not provide this export; record its settings from its existing interface before installing.

## Confirm preservation after updating

Compare the configured clock and voltage exactly with your record. Check fan policy, both pool routes, network identity, and display settings. A blank or masked password field does not mean the password was erased. The pool editor retains the saved password until you deliberately enter a replacement.

Application and WWW uploads target their respective application and SPIFFS partitions; neither is intended to replace the NVS partition. Factory/configuration images and erase-flash operations can overwrite settings and are different procedures.

The firmware's existing thermal protection remains active. An overheat recovery can reduce stored voltage/frequency and change fan settings. That is a protection response, distinct from update preservation; inspect the reported fault/overheat status before attributing a changed operating point to an upgrade. A normal pause temporarily powers down the ASIC and resumes from the saved operating point without writing a new clock/voltage preset.

## Coinbase diagnostics

Primary and fallback coinbase decoding default to **off when the corresponding saved setting is absent**. Explicitly saved decoder settings are retained at startup. The 5tratMUX connection preset explicitly switches primary decoding off when you save it. Browser-side coinbase/reward diagnostics also default off and are a local browser preference.

Decoded coinbase outputs are diagnostic information; they do not verify pool or MUX payouts.

If storage cannot be read, follow [Recovery](recovery.md). The firmware deliberately does not erase storage or manufacture a default board identity to continue booting.
