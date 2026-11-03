#pragma once
namespace FiveTratumAutotune {
// True only for the lifetime of one explicit normal cold-start call. A manual
// cancellation cannot drop this guard midway through regulator initialization.
bool preserveRecoveryFaults();
struct RecoveryStartupGuard { RecoveryStartupGuard(); ~RecoveryStartupGuard(); };
}
