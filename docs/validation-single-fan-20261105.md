# Fitted-fan qualification, 5 November 2026

Candidate releases are NerdQAxe++ Beta14 and NerdOctAxe Gamma Beta12. Gamma firmware is unchanged because it uses a different fan driver.

The EMC2302 status registers are read-to-clear and carry per-channel fault bits. The revised driver takes one shared status snapshot before evaluating either channel; it preserves a sibling's fault until that channel has been evaluated. Required-fan policy is an explicit, separately stored, identity-bound declaration. It never changes fan outputs and never classifies zero RPM as optional automatically.

The completion record will identify the built source, exact checks, packages and any live tests. No physical single-fan bench claim is made by this draft.
