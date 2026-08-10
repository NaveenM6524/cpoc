#include <stdio.h>

#include "logging.h"
#include "util.h"
#include "common.h"

void logEvent(const char *user, const char *action, const char *detail)
{
    ensureDataDir();

    FILE *fp = fopen(AUDIT_LOG, "a");
    if (!fp) {
        fprintf(stderr, "warning: could not write to audit log\n");
        return;
    }

    char timestamp[MAX_TIMESTAMP_LEN];
    getCurrentTimestamp(timestamp, sizeof(timestamp));

    fprintf(fp, "%s | %s | %s | %s\n", timestamp, user, action, detail);
    fclose(fp);
}
