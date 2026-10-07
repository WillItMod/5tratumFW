# Gamma Beta 4 validation

Version: `5tratumFW-0.1.0-beta.4`. Development BETA for Gamma PCB601/602 only.

This build places the operating-point confirmation immediately below the manual Review button or the selected tuning slot controls. Review remains read-only; only explicit Apply changes a setting. Manual input/mode changes invalidate the earlier review. Pool Apply and the ten pool/tuning slots retain their previous behavior. No preset operating points or hardware control changes are introduced.

The preceding Beta3 pair passed the [601 OTA, pool and power-scheduler checks](validation-beta3.md). That is separate evidence. Beta4 full build, paired OTA and real-browser verification are pending at this source checkpoint. The source archive and image manifest bind the exact build commit.

The component focused tests and all121 frontend tests passed before the full build. Linux CI identified a misleading-indentation warning in a pool-scheduler test fixture; braces now clarify the existing loop without changing production behavior.

Gamma602 remains unflashed. Physical display readability, sustained soak and fault/recovery qualification remain incomplete. Independent ASIC jobs and richer Gamma coin/block injection are not implemented.
