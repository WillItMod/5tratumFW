#ifndef FIVE_TRATUM_STATUS_REPORT_H
#define FIVE_TRATUM_STATUS_REPORT_H

#include "five_tratum_mux_status.h"

#define FIVE_TRATUM_STATUS_MAX_BYTES 2048

typedef struct {
    FiveTratumMuxSnapshot peer;
    bool using_fallback;
    bool protocol_v1;
    bool coherent;
    uint64_t uptime_seconds;
} FiveTratumStatusReport;

// The binding is produced by operating_profiles_binding(). No settings,
// credentials, endpoint names or fabricated per-chip samples are serialized.
// Returned objects are owned by the caller; allocation failure returns NULL.
cJSON *five_tratum_status_report(const cJSON *binding, const FiveTratumStatusReport *snapshot);

#endif
