#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "distribution.h"
#include "inventory.h"
#include "util.h"
#include "logging.h"

/* comparator for qsort - sorts batches oldest-expiry-first, which is the
 * heart of the FEFO greedy algorithm below */
static int compareByExpiry(const void *a, const void *b)
{
    const Medicine *const *ma = a;
    const Medicine *const *mb = b;
    return compareDates((*ma)->expiryDate, (*mb)->expiryDate);
}

/* atomic append, same pattern as supply.c's appendSupplyRecord */
static OpStatus appendRequestRecord(SupplyRequest *r)
{
    int maxId = 0;
    FILE *in = fopen(REQUESTS_FILE, "r");
    FILE *out = fopen(REQUESTS_TMP, "w");
    if (!out) {
        if (in) fclose(in);
        return OP_FILE_ERROR;
    }

    if (in) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), in)) {
            int id;
            if (sscanf(line, "%d|", &id) == 1 && id > maxId) {
                maxId = id;
            }
            fputs(line, out);
        }
        fclose(in);
    }

    r->requestId = maxId + 1;
    fprintf(out, "%d|%s|%d|%d|%d|%s\n",
            r->requestId, r->medicineName, r->requestedQty, r->fulfilledQty,
            (int)r->status, r->timestamp);
    fclose(out);

    if (rename(REQUESTS_TMP, REQUESTS_FILE) != 0) {
        return OP_FILE_ERROR;
    }
    return OP_SUCCESS;
}

OpStatus distributionProcessRequest(const char *medicineName, int requestedQty,
                                     const char *requestedBy, SupplyRequest *outResult)
{
    if (!medicineName || medicineName[0] == '\0' || requestedQty <= 0) {
        return OP_INVALID_INPUT;
    }
    if (containsDelimiter(medicineName)) {
        return OP_INVALID_INPUT;
    }

    Medicine **batches;
    int count = inventoryFindAllByName(medicineName, &batches);

    /* greedy FEFO: sort every matching batch by expiry, then drain the
     * earliest-expiring batch first, spilling into the next batch only
     * once the current one runs dry */
    if (count > 0) {
        qsort(batches, (size_t)count, sizeof(Medicine *), compareByExpiry);
    }

    int remaining = requestedQty;
    int fulfilled = 0;

    for (int i = 0; i < count && remaining > 0; i++) {
        int available = batches[i]->quantity;
        if (available <= 0) {
            continue;
        }
        int take = (remaining < available) ? remaining : available;

        if (inventoryDecreaseStock(batches[i]->id, take, requestedBy) == OP_SUCCESS) {
            fulfilled += take;
            remaining -= take;
        }
    }
    free(batches);

    SupplyRequest r;
    strncpy(r.medicineName, medicineName, MAX_NAME_LEN - 1);
    r.medicineName[MAX_NAME_LEN - 1] = '\0';
    r.requestedQty = requestedQty;
    r.fulfilledQty = fulfilled;
    getCurrentTimestamp(r.timestamp, sizeof(r.timestamp));

    if (fulfilled == 0) {
        r.status = REQ_REJECTED;
    } else if (fulfilled < requestedQty) {
        r.status = REQ_PARTIAL;
    } else {
        r.status = REQ_FULFILLED;
    }

    if (appendRequestRecord(&r) != OP_SUCCESS) {
        return OP_FILE_ERROR;
    }

    const char *statusText = (r.status == REQ_FULFILLED) ? "FULFILLED" :
                              (r.status == REQ_PARTIAL) ? "PARTIAL" : "REJECTED";
    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "request id=%d name=%s requested=%d fulfilled=%d status=%s",
              r.requestId, medicineName, requestedQty, fulfilled, statusText);
    logEvent(requestedBy, "DISTRIBUTION_REQUEST", detail);

    if (outResult) {
        *outResult = r;
    }
    return OP_SUCCESS;
}
