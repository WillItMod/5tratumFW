# Pool routing and 5tratMUX

**Pool routing** contains both **Direct pool** and **5tratMux** connection editors. MUX setup is a miner-side connection preset within this page. Selecting a mode changes the draft; saving and restarting are separate actions.

## Direct pool

Edit the primary and fallback routes using the information supplied by your pool: host, port, worker/user identity, password, and supported protocol/security options. The primary is used first; the fallback is used when the primary cannot be reached. Ensure both routes use your intended payout/account identity, including the retained fallback. Factory wallet settings must be replaced before mining for your own rewards.

A saved password remains unchanged until you enter a replacement. Advanced settings include SV1/SV2 options, security, extranonce subscription, suggested difficulty, and optional coinbase decoding. Choose only options supported by the destination. Suggested difficulty is a request the pool may ignore; `0` disables the request. Coinbase decoding defaults off and is a diagnostic, not payout verification.

Choose **Save pool settings**, then **Restart device** to apply the saved connection. Unsaved draft edits do not take effect. After restart, check fresh telemetry and the pool's own worker view.

## Named pool slots in the next source layout

Gamma Beta7, QAxe Beta5 and OctAxe Beta3 source candidates place the **Primary** and **Secondary** connection editors above the ten saved slots. They are not yet released. On Nerd Dual pool, Primary and Secondary correspond to native A and B; in failover, Secondary is standby. Gamma's Secondary is its fallback route.

Use **Save pool settings** to persist connection edits. Then select a slot, enter its name and choose **Save to slot** beside that route's save button. This stores the miner's saved connection and credentials; unsaved connection edits are refused with **Save pool settings first**. Each stored row has **Apply to Primary** and **Apply to Secondary**, followed by **Rename** and **Clear**. Rename changes only the slot name and retains its saved connection and password. An Apply to a destination with unsaved edits is refused. Secondary continues to use the existing `poolTarget: "fallback"` API; this naming change does not add a third connection or per-ASIC routing.

## Connect a Gamma to 5tratMUX

1. Run and configure a compatible 5tratMUX service separately. Configure payout identities and upstream routes there. This firmware repository does not install or deploy the service.
2. In **Pool routing**, select **5tratMux**. Enter its hostname, port, and a unique worker identity. Enter only a host in **MUX host**, without a URI scheme, path, or `:port`. The default Stratum port is `7331`; use the actual port of your service.
3. Keep the saved password or deliberately provide a replacement. Worker identities must not contain spaces and must be at most 128 characters.
4. Choose **Review MUX connection**, inspect the exact primary endpoint, worker identity, and password action, then **Save MUX connection**.
5. Choose **Restart device** separately. Confirm its worker session and intended upstream route in the MUX console.

Saving the MUX preset sets the primary connection to **Stratum V1 / TCP**, extranonce subscription on, suggested difficulty `0`, and coinbase decoding off. It selects the primary route. It does not write Wi-Fi, clock, voltage, or fan fields. Fallback values are retained, and unsaved fallback draft edits are not included in a MUX save. A later fallback selection must be checked as its own route.

The TCP preset is intended for your configured local connection. Worker identity is the downstream miner identity; payout/account identities belong in MUX's upstream settings.

## Interpret connection evidence

The web page distinguishes saved configuration, restart status, selected primary/fallback route, stale telemetry, and increases in device accepted-share counters. Matching saved settings alone do not establish a live MUX connection. Increased share counters show device mining activity, not proof of a particular MUX upstream route or payout. Use the MUX worker/session view and your pool's own account information for those checks.

The Gamma OLED can identify a connected MUX peer only after a valid, fresh `mining.5tratum.status` advertisement arrives from the actual current Stratum V1 connection. It does not infer a MUX badge from the saved host, port, worker name, or selected preset. The advertisement expires at **90 seconds**, and disconnect, reconnect, or a protocol switch clears it. An advertisement from a previous connection cannot refresh the new session. A primary advertisement cannot identify an ordinary fallback connection as MUX.

The matching MUX server extension was **not deployed during repository preparation**. A MUX server without that extension can mine normally while producing no OLED MUX identification. Check the firmware release's validation record before claiming a live end-to-end advertisement test. See [Peer status protocol](mux-status-v1.md) for the strict message contract.

Peer status is an advertisement, not cryptographic authentication, upstream health, or proof of accepted work. This Gamma firmware does not implement or validate per-chip independent mining or dedicated per-chip multi-coin work. The peer advertisement explicitly declares shared chain-broadcast work and `independentWorkAssignment: false`.
