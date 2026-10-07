These host tests compile the actual `main/nvs_config.c` and
`main/device_config.c` with minimal ESP-IDF/FreeRTOS stubs. They run without
ESP-IDF, network access, a connected miner, or flashing:

```sh
python3 test/host/run_gamma_boot_tests.py
```

They verify that only persisted board identities 601/602 reach settings writer
startup, every flash-init error is returned without NVS erase, and stored clocks,
voltage, fan policy, pools, Wi-Fi, empty strings, legacy keys, and unknown keys
survive settings loading unchanged. A source-level contract checks that the
hardware initialization calls follow both boot guards.

Both primary and fallback Decode Coinbase options default to off when their
keys are absent. All four combinations of explicitly saved options remain
unchanged during startup, without writing defaults or migrating keys.

The fake scheduler does not run firmware tasks. These tests do not validate
electrical behavior, ESP-IDF drivers, thermal control, OTA compatibility, or the
OLED layout on physical hardware. Existing thermal and frequency management
remain responsible for safe operation after a supported board starts.
