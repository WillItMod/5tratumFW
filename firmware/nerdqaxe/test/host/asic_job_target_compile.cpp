#include "asic_job_selector.h"

// Compile this file with the ESP32-S3 toolchain; never link this test probe into
// the miner. The core must stay bounded, and belongs in retained board storage
// rather than an 8 KiB task stack when the physical transport is implemented.
static_assert(sizeof(FiveTratumJobs::AsicJobRegistry) <= 24 * 1024,
              "Four-chip context storage exceeded the prototype budget");
static_assert(sizeof(FiveTratumJobs::AsicJobSelector) <= 2 * 1024,
              "Four-chip pending work exceeded the prototype budget");

FiveTratumJobs::SelectionResult compileJobSelection(
        FiveTratumJobs::AsicJobRegistry &registry,
        const FiveTratumJobs::WorkToken &owner,
        const FiveTratumJobs::JobStamp &job,
        FiveTratumJobs::IsolatedWorkTransport &transport) {
    FiveTratumJobs::AsicJobSelector selector(registry);
    selector.stage(owner, job);
    return selector.dispatchNext(transport);
}
