#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reports.h"
#include "inventory.h"
#include "util.h"
#include "logging.h"
#include "common.h"

void reportStock(FILE *out, const char *viewer)
{
    Medicine **items;
    int count = inventoryGetAll(&items);

    fprintf(out, "\n%-5s %-24s %-12s %-8s %-12s %-8s\n",
           "ID", "Name", "Batch", "Qty", "Expiry", "Reorder");
    fprintf(out, "---------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        fprintf(out, "%-5d %-24s %-12s %-8d %-12s %-8d\n",
               items[i]->id, items[i]->name, items[i]->batch,
               items[i]->quantity, items[i]->expiryDate, items[i]->reorderLevel);
    }
    if (count == 0) {
        fprintf(out, "(no inventory records)\n");
    }
    free(items);

    logEvent(viewer, "VIEW_REPORT", "stock report");
}

void reportLowStock(FILE *out, const char *viewer)
{
    Medicine **items;
    int count = inventoryGetAll(&items);
    int shown = 0;

    fprintf(out, "\n%-5s %-24s %-12s %-8s %-8s\n", "ID", "Name", "Batch", "Qty", "Reorder");
    fprintf(out, "--------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        if (items[i]->quantity <= items[i]->reorderLevel) {
            fprintf(out, "%-5d %-24s %-12s %-8d %-8d\n",
                   items[i]->id, items[i]->name, items[i]->batch,
                   items[i]->quantity, items[i]->reorderLevel);
            shown++;
        }
    }
    if (shown == 0) {
        fprintf(out, "(nothing at or below reorder level)\n");
    }
    free(items);

    logEvent(viewer, "VIEW_REPORT", "low stock report");
}

void reportExpiry(FILE *out, int daysWindow, const char *viewer)
{
    Medicine **items;
    int count = inventoryGetAll(&items);

    char today[MAX_DATE_LEN];
    getCurrentDate(today, sizeof(today));
    int td, tm, ty;
    sscanf(today, "%2d-%2d-%4d", &td, &tm, &ty);
    long todayJDN = dateToJDN(td, tm, ty);

    fprintf(out, "\n-- Already expired --\n");
    int expiredShown = 0;
    for (int i = 0; i < count; i++) {
        int d, m, y;
        sscanf(items[i]->expiryDate, "%2d-%2d-%4d", &d, &m, &y);
        long diff = dateToJDN(d, m, y) - todayJDN;
        if (diff < 0) {
            fprintf(out, "  [%d] %s batch %s expired on %s (%ld day(s) ago)\n",
                   items[i]->id, items[i]->name, items[i]->batch,
                   items[i]->expiryDate, -diff);
            expiredShown++;
        }
    }
    if (expiredShown == 0) {
        fprintf(out, "  (none)\n");
    }

    fprintf(out, "\n-- Expiring within %d day(s) --\n", daysWindow);
    int soonShown = 0;
    for (int i = 0; i < count; i++) {
        int d, m, y;
        sscanf(items[i]->expiryDate, "%2d-%2d-%4d", &d, &m, &y);
        long diff = dateToJDN(d, m, y) - todayJDN;
        if (diff >= 0 && diff <= daysWindow) {
            fprintf(out, "  [%d] %s batch %s expires %s (in %ld day(s))\n",
                   items[i]->id, items[i]->name, items[i]->batch,
                   items[i]->expiryDate, diff);
            soonShown++;
        }
    }
    if (soonShown == 0) {
        fprintf(out, "  (none)\n");
    }
    free(items);

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "expiry report window=%d days", daysWindow);
    logEvent(viewer, "VIEW_REPORT", detail);
}

void reportSupplyHistory(FILE *out, const char *viewer)
{
    FILE *fp = fopen(SUPPLY_FILE, "r");
    fprintf(out, "\n%-5s %-24s %-8s %-16s %-12s %-20s\n",
           "ID", "Medicine", "Qty", "Supplier", "Admin", "Timestamp");
    fprintf(out, "---------------------------------------------------------------------------------\n");

    if (fp) {
        char line[MAX_LINE_LEN];
        int shown = 0;
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            int id, qty;
            char name[MAX_NAME_LEN], supplier[MAX_SUPPLIER_LEN];
            char admin[MAX_USERNAME_LEN], ts[MAX_TIMESTAMP_LEN];

            if (sscanf(line, "%d|%63[^|]|%d|%63[^|]|%31[^|]|%19[^\n]",
                       &id, name, &qty, supplier, admin, ts) == 6) {
                fprintf(out, "%-5d %-24s %-8d %-16s %-12s %-20s\n", id, name, qty, supplier, admin, ts);
                shown++;
            }
        }
        fclose(fp);
        if (shown == 0) {
            fprintf(out, "(no supply history)\n");
        }
    } else {
        fprintf(out, "(no supply history)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "supply history");
}

void reportDistributionHistory(FILE *out, const char *viewer)
{
    static const char *statusNames[] = { "FULFILLED", "PARTIAL", "REJECTED" };

    FILE *fp = fopen(REQUESTS_FILE, "r");
    fprintf(out, "\n%-5s %-24s %-10s %-10s %-10s %-20s\n",
           "ID", "Medicine", "Requested", "Fulfilled", "Status", "Timestamp");
    fprintf(out, "---------------------------------------------------------------------------------\n");

    if (fp) {
        char line[MAX_LINE_LEN];
        int shown = 0;
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            int id, reqQty, fulQty, status;
            char name[MAX_NAME_LEN], ts[MAX_TIMESTAMP_LEN];

            if (sscanf(line, "%d|%63[^|]|%d|%d|%d|%19[^\n]",
                       &id, name, &reqQty, &fulQty, &status, ts) == 6) {
                const char *statusText = (status >= 0 && status <= 2) ? statusNames[status] : "?";
                fprintf(out, "%-5d %-24s %-10d %-10d %-10s %-20s\n",
                       id, name, reqQty, fulQty, statusText, ts);
                shown++;
            }
        }
        fclose(fp);
        if (shown == 0) {
            fprintf(out, "(no distribution history)\n");
        }
    } else {
        fprintf(out, "(no distribution history)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "distribution history");
}

void reportAccountability(FILE *out, const char *viewer)
{
    FILE *fp = fopen(AUDIT_LOG, "r");
    fprintf(out, "\n-- Accountability / Audit Trail --\n");

    if (fp) {
        char line[MAX_LINE_LEN];
        int shown = 0;
        while (fgets(line, sizeof(line), fp)) {
            fputs(line, out);
            shown++;
        }
        fclose(fp);
        if (shown == 0) {
            fprintf(out, "(audit log is empty)\n");
        }
    } else {
        fprintf(out, "(audit log is empty)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "accountability report");
}
