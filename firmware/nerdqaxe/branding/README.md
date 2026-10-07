# 5tratumFW artwork

`5tratumfw-logo.png` is the approved full colour 5tratumFW logo, including its own cyan/magenta lettering, anvil, pickaxes, ASIC and linked cubes. Source SHA256: `90b7f3f9fee2d758f974d07cbfc6ba386614f387fc4c0855fb12957479f15158`.

Run `python3 tools/generate_display_logo.py` with Pillow to encode the original artwork into fixed RGB565+alpha LVGL assets and the smaller browser PNG. Startup and the recurring screen slide use the complete logo; telemetry headers use its original lettering and underline. These are firmware assets with no runtime PNG decoder or image allocation. The embedded assets consume123,120bytes of flash.

This artwork is separate from the 5tratumOS emblem. The website uses the same full logo and does not add a second text wordmark beside it.
