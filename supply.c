#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "supply.h"
#include "inventory.h"
#include "util.h"
#include "logging.h"

/* atomic append: copy every existing row into a temp file, add the new
 * one, then rename() over the original so a crash mid-write never leaves
 * a half-written supply.dat behind. also derives the next transaction id
 * from the highest id seen so far. */
static OpStatus appendSupplyRecord(SupplyTransaction *t)
{
    int maxId = 0;
    FILE *in = fopen(SUPPLY_FILE, "r");
    FILE *out = fopen(SUPPLY_TMP, "w");
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

    t->id = maxId + 1;
    fprintf(out, "%d|%s|%d|%s|%s|%s\n",
            t->id, t->medicineName, t->quantity, t->supplierName,
            t->addedByAdmin, t->timestamp);
    fclose(out);

    if (rename(SUPPLY_TMP, SUPPLY_FILE) != 0) {
        return OP_FILE_ERROR;
    }
    return OP_SUCCESS;
}

OpStatus supplyProcess(const char *medicineName, const char *batch, int quantity,
                        const char *expiryDateRaw, int reorderLevel,
                        const char *supplierName, const char *adminUsername)
{
    if (!medicineName || medicineName[0] == '\0' ||
        !batch || batch[0] == '\0' ||
        !supplierName || supplierName[0] == '\0') {
        return OP_INVALID_INPUT;
    }
    if (containsDelimiter(medicineName) || containsDelimiter(batch) || containsDelimiter(supplierName)) {
        return OP_INVALID_INPUT;
    }
    if (quantity <= 0 || reorderLevel < 0) {
        return OP_INVALID_INPUT;
    }

    char normalizedExpiry[MAX_DATE_LEN];
    if (!normalizeDate(expiryDateRaw, normalizedExpiry) || !isValidDate(normalizedExpiry)) {
        return OP_INVALID_INPUT;
    }

    Medicine *existing = inventoryFindExact(medicineName, batch, normalizedExpiry);
    OpStatus status;
    int recordId;

    if (existing) {
        status = inventoryIncreaseStock(existing->id, quantity, adminUsername);
        recordId = existing->id;
    } else {
        status = inventoryAddNew(medicineName, batch, quantity, normalizedExpiry,
                                  reorderLevel, adminUsername, &recordId);
    }

    if (status != OP_SUCCESS) {
        return status;
    }

    SupplyTransaction t;
    (void)recordId; /* traceability lives in the inventory audit trail */
    strncpy(t.medicineName, medicineName, MAX_NAME_LEN - 1);
    t.medicineName[MAX_NAME_LEN - 1] = '\0';
    t.quantity = quantity;
    strncpy(t.supplierName, supplierName, MAX_SUPPLIER_LEN - 1);
    t.supplierName[MAX_SUPPLIER_LEN - 1] = '\0';
    strncpy(t.addedByAdmin, adminUsername, MAX_USERNAME_LEN - 1);
    t.addedByAdmin[MAX_USERNAME_LEN - 1] = '\0';
    getCurrentTimestamp(t.timestamp, sizeof(t.timestamp));

    if (appendSupplyRecord(&t) != OP_SUCCESS) {
        return OP_FILE_ERROR;
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "supply id=%d name=%s batch=%s qty=%d supplier=%s",
             t.id, medicineName, batch, quantity, supplierName);
    logEvent(adminUsername, "SUPPLY_RECEIVED", detail);

    return OP_SUCCESS;
}
