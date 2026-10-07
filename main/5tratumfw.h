#ifndef FIVE_STRATUM_FW_H
#define FIVE_STRATUM_FW_H

#include <stdbool.h>
#include <string.h>

#define FIVE_STRATUM_FW_NAME "5tratumFW"

// The first pilot uses the upstream Gamma 601/602 hardware profiles only.
// Require an exact, persisted identity; defaults and custom profiles are unsafe.
static inline bool five_stratum_fw_board_supported(const char *board_version)
{
    return board_version != NULL &&
           (strcmp(board_version, "601") == 0 || strcmp(board_version, "602") == 0);
}

#endif
