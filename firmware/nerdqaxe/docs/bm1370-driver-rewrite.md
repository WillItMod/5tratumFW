# BM1370 driver rewrite

The objective is simultaneous independent mining on the four BM1370 ASICs in
the test QAxe++, using the existing hardware. Each ASIC must retain its own
pool job while the other ASICs receive different jobs. Whole-chain time slicing
does not meet that objective.

Rewriting the driver gives the firmware explicit control over packet encoding,
dispatch and response validation. It cannot create a command that the silicon
does not implement. No reviewed source establishes an addressed BM1370 mining
job or a local job-acceptance gate that retains the preceding job. The current
capabilities therefore remain `independentWorkAssignment:false` and
`workTargeting:"chain-broadcast"`.

## Scope

The first rewrite separates a fixed-size, hardware-independent BM1370 protocol
layer from the shared driver. Normal jobs retain the existing Nerd 88-byte
packet, wire job-ID mapping and header byte order. Chip-specific job requests
must report unsupported without sending any bytes. Register addressing and
job IDs are separate concepts.

The diagnostic response parser accepts only complete frames with the captured
CRC5 layout and recognized response types. It handles fragments, noise and
resynchronization in bounded storage. Reset and transport retirement clear
partial frames. Normal initialization, saved clock, voltage, version rolling,
fan policy and shared power protections retain their established paths.

The rewrite is opt-in at compile time until it has been checked on our physical
QAxe. It introduces no guessed selector, modified version mask, added job
address byte, `0x31` job experiment or arbitrary register write. It is a driver
foundation, not a completed split-mining implementation.

The option is `FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER`, default `OFF`. Its explicit
0/1 definition is exported to dependent board code: the allocator and driver
must agree on the class layout. The legacy receive path remains selected when
the option is off. The normal initialization UART sequence is unchanged in
both modes.

## Software checks

```sh
python3 test/host/test_bm1370_protocol.py
python3 test/host/test_mining_driver.py
python3 test/host/test_asic_capture_check.py
python3 test/host/test_mining_control.py
python3 test/host/test_mining_schedule.py
python3 test/host/test_hashrate_monitor.py
python3 test/host/test_asic_job_registry.py
python3 test/host/test_asic_job_selector.py
python3 test/host/test_capabilities_report.py
```

The pure codec passed eight check groups with address/undefined-behavior
sanitizers: 1,024 existing-source job packet comparisons, all 65,536 addressed
register-read combinations, five published response frames, 360 single-bit
checksum mutations, fragmentation/noise/overlap/reset handling and one million
random input bytes. Fixed-storage checks reject C++ heap allocation. It has no
UART, clock or RTOS dependency.

The integration harness compiles the actual subclass, board and power methods
in both modes. It checks 2,048 job counter values, unchanged raw job packets,
short writes, unsupported chip requests with zero UART writes, five captured
responses at fifty splits, 440 single-bit frame mutations, observed-topology
bounds, one-read timeout budgets and parser/tail retirement. Eighteen legacy
power/startup/fault scenarios and eight diagnostic scenarios pass. All 117
initialization packets match between modes. The capture checker additionally
passes twenty-one tests, including complete ON/OFF sender/result equality.

These checks replace UART, GPIO, regulator and RTOS boundaries with host
fixtures. None proves physical chip ownership, pipeline retirement or
concurrent mining. The original a7 target/UI checks remain recorded separately.

Both modes also compile for `NERDQAXEPLUS2` using the pinned ESP-IDF 5.5.3
container. The diagnostic application is 3,289,344 bytes; the default-off
application is 3,287,024 bytes. Both fit the 4 MiB application partition with
22% free. Compile commands confirm the same explicit flag in the BM1370
component and every inspected board allocator (`nerdqaxeplus2`, `nerdaxegamma`
and `q1370`). These are compile-only artifacts carrying the existing a7 version,
not paired releases to install. Local binary hashes and commands are retained
in the ignored `.cache/bm1370-driver-rewrite/` directory.

## Reusing existing dual-pool support

QAxe dual-pool mode already has two live Stratum tasks, separate job context
for each pool and result submission to the originating pool. This is useful
infrastructure for independent mining. Its current selector returns a pool
index, not a chip index.

In our a7-derived source, the path is:

1. `StratumManagerDualPool::getNextActivePool()` selects pool 0 or 1 using the
   configured balance and which pools have valid notifications.
2. `create_jobs_task.cpp` takes the selected pool's mining information, builds
   one `bm_job` containing `pool_id`, then calls `asics->sendWork()` once.
3. The driver sends the existing unaddressed `0x21` full-header job. The
   shared chain ticket mask is selected from both pool difficulties.
4. The result task restores the immutable job by its returned wire ID,
   verifies the nonce and submits using the job's original `pool_id`.

The actual selector method passed a bounded host exercise with notifications
held valid: 1,000 selections produced 500/500 at 50/50 and 750/250 at 75/25.
When only one pool was valid, all 1,000 selections chose that pool. These count
selector calls, not actual mining time, successful dispatches or measured
hashrate. Notifications can wake the job task outside its regular timer, and
a later job-validity check can skip a selected pool. The current per-pool
hashrate display estimates the total rate multiplied by the configured balance.

Keeping job records from both pools permits attribution of late responses.
It does not establish that different ASICs retain different jobs. Brief A/B
response overlap can also arise from the work pipeline or UART latency.
The rewritten codec preserves the original packet bytes and wire-ID mapping,
so the existing pool context and submission path remain available.

This trace was checked against local upstream base
`8b45522a6695c6bbccc3d032370fe08a29856d80` and separately against upstream
develop pinned on 7 October 2026 at
`d129cb46134a4cd5cdf2298be55672fd09e7f3d5`. Both use the same shared-chain
dispatch structure:

- [Dual-pool selection](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/d129cb46134a4cd5cdf2298be55672fd09e7f3d5/main/stratum/stratum_manager_dual_pool.cpp#L52).
- [One selected job sent to the driver](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/d129cb46134a4cd5cdf2298be55672fd09e7f3d5/main/tasks/create_jobs_task.cpp#L344).
- [Submission through the saved pool context](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/d129cb46134a4cd5cdf2298be55672fd09e7f3d5/main/tasks/ASIC_result_task.cpp#L88).

The active SV2 mode explicitly uses Extended channels, reaching that same
dispatcher. Standard-channel one-shot code exists but is disabled. Latest
upstream also adds version-rolling compatibility guards that our local base
does not have; those are separate from chip targeting. No upstream SV2 change
was merged as part of this rewrite.

The a9 bench now supplies normal-job/result capture from our own QAxe, including
independent SHA-256d matches for two returned nonces. The remaining evidence
needed is a source-supported method of local work acceptance. Two sessions
solve the network side; a physical chip selector is still unqualified.

## Why the generic command names are insufficient

The existing driver calls its job header `TYPE_JOB | GROUP_SINGLE | CMD_WRITE`,
which equals `0x21`. Its payload starts with a work ID and midstate count; there
is no chip-address field. A captured single-chip job and the documented S21 Pro
chain jobs use the same structure. The generic `GROUP_SINGLE` name does not
establish that a job is delivered to one chip.

- [Captured BM1370 transaction](https://github.com/256foundation/mujina/blob/980128574d8bb8b9619b0081b26e18eb4d0c5e31/mujina-miner/src/asic/bm13xx/test_data.rs#L32).
- [Full mining-job fields](https://github.com/256foundation/mujina/blob/980128574d8bb8b9619b0081b26e18eb4d0c5e31/mujina-miner/src/asic/bm13xx/REFERENCE.md#L497).
- [Register operations have a destination; job builders do not](https://github.com/GPTechinno/bm13xx-rs/blob/d00c086bc66e6d4d4fa47fa6191ae40e7e566c0d/bm13xx-protocol/src/command.rs#L211).

These are open-source implementations and community hardware captures, not a
manufacturer specification. The full S21 Pro raw UART capture was not present
in the inspected repository. Third-party fixtures are kept separate from our
own physical observations.

The additional bank-selection review found host DDR/FPGA buffer switching in
the [vendor job sender](https://github.com/HashSource/bitmain_antminer_binaries/blob/0a78d7d123ed3b720f074e6d524ce5f96012034d/S21pro/single_board_test.dec/dhash_send_job%40C323C.c),
not a per-ASIC resident job-bank selector. The reviewed work-disable helper
also controls FPGA registers without an ASIC address. Core-mailbox enable
calls belong to reset/bring-up; the
[Mujina mailbox evidence](https://github.com/256foundation/mujina/blob/980128574d8bb8b9619b0081b26e18eb4d0c5e31/mujina-miner/src/asic/bm13xx/REFERENCE.md#L1240)
contains broadcast captures. Neither that evidence nor the named PCE/force-core
setters establishes a chip retaining job A while another accepts job B.
These names do not justify a targeting write on the live QAxe.

## Required hardware proof

1. Check normal chain jobs and checksum-valid responses on the identified
   QAxe, preserving its 500 MHz / 1130 mV operating point and rolling setting.
2. Establish a source-supported candidate for local work acceptance before
   sending a new command. Register read addressing alone is insufficient.
3. Broadcast immutable job A, then deliver distinct job B to one ASIC through
   that mechanism. Reconstruct and independently hash every captured result.
   The selected ASIC must produce B while the other three continue producing
   A after a measured, justified pipeline window.
4. Repeat for each ASIC, then demonstrate four different concurrent jobs,
   pool reconnects, clean jobs, stale results and work-ID reuse. Shared pause,
   reset and fault handling must retire every child route.

Suppressing nonce reports, accepting only one ASIC's results, four sockets,
four worker names, host mocks or a capability flag cannot pass this test.
The existing per-ASIC job registry and selector remain disconnected from live
dispatch until the physical transport is qualified.

BM1370 nonce responses contain a core/counter value rather than an explicit
chip-address field. The existing Nerd nonce-to-chip calculation is a
configuration-dependent estimate. Register responses do contain an address.
Normal attribution must be checked against actual chain enumeration and nonce
offset behavior before it is used as evidence of isolation.

## Current bench state

The QAxe now runs paired a9 application and WWW images with the diagnostic
driver and bounded passive capture enabled. The 7 October 2026 bench retained
500 MHz, 1130 mV and version-rolling frequency 25011, observed approximately
4 TH/s and fresh accepted shares, and independently verified two real nonce/header
pairs. Its physical PCB revision remains unidentified. No allocation draft or
chip-targeted command was installed. See [a9 validation](validation-a9.md) and the
[targeting investigation](asic-work-targeting-investigation.md).
