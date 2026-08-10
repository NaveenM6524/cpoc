#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inventory.h"
#include "util.h"
#include "logging.h"

/* hash table: array of chain heads, keyed by record id.
 * algorithm: hash table + separate chaining */
static Medicine *table[HASH_TABLE_SIZE];
static int nextRecordId = 1;

static unsigned int hashId(int id)
{
    return (unsigned int)id % HASH_TABLE_SIZE;
}

static void insertNode(Medicine *m)
{
    unsigned int slot = hashId(m->id);
    m->next = table[slot];
    table[slot] = m;
}

void inventoryInit(void)
{
    ensureDataDir();
    memset(table, 0, sizeof(table));
    nextRecordId = 1;

    FILE *fp = fopen(INVENTORY_FILE, "r");
    if (!fp) {
        return; /* first run - nothing to load yet */
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') {
            continue;
        }

        Medicine *m = malloc(sizeof(Medicine));
        if (!m) {
            continue;
        }

        int id, quantity, reorderLevel;
        char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN], expiry[MAX_DATE_LEN];

        /* defensive/fault-tolerant parsing: skip and log malformed rows
         * instead of crashing on a corrupt data file */
        int fields = sscanf(line, "%d|%63[^|]|%31[^|]|%d|%10[^|]|%d",
                             &id, name, batch, &quantity, expiry, &reorderLevel);

        if (fields != 6 || quantity < 0 || reorderLevel < 0 || !isValidDate(expiry)) {
            free(m);
            logEvent("SYSTEM", "LOAD_WARNING", "corrupt inventory row skipped");
            continue;
        }

        m->id = id;
        strncpy(m->name, name, MAX_NAME_LEN - 1);
        m->name[MAX_NAME_LEN - 1] = '\0';
        strncpy(m->batch, batch, MAX_BATCH_LEN - 1);
        m->batch[MAX_BATCH_LEN - 1] = '\0';
        m->quantity = quantity;
        strncpy(m->expiryDate, expiry, MAX_DATE_LEN - 1);
        m->expiryDate[MAX_DATE_LEN - 1] = '\0';
        m->reorderLevel = reorderLevel;
        m->next = NULL;

        insertNode(m);
        if (id >= nextRecordId) {
            nextRecordId = id + 1;
        }
    }
    fclose(fp);
}

void inventoryFreeAll(void)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        Medicine *cur = table[i];
        while (cur) {
            Medicine *doomed = cur;
            cur = cur->next;
            free(doomed);
        }
        table[i] = NULL;
    }
}

Medicine *inventoryFindById(int id)
{
    for (Medicine *cur = table[hashId(id)]; cur; cur = cur->next) {
        if (cur->id == id) {
            return cur;
        }
    }
    return NULL;
}

Medicine *inventoryFindExact(const char *name, const char *batch, const char *expiryDate)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
            if (strcmp(cur->name, name) == 0 &&
                strcmp(cur->batch, batch) == 0 &&
                strcmp(cur->expiryDate, expiryDate) == 0) {
                return cur;
            }
        }
    }
    return NULL;
}

int inventoryFindAllByName(const char *name, Medicine ***outArray)
{
    int matchCount = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
            if (strcmp(cur->name, name) == 0 && cur->quantity > 0) {
                matchCount++;
            }
        }
    }
    if (matchCount == 0) {
        *outArray = NULL;
        return 0;
    }

    Medicine **arr = malloc((size_t)matchCount * sizeof(Medicine *));
    if (!arr) {
        *outArray = NULL;
        return 0;
    }

    int idx = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
            if (strcmp(cur->name, name) == 0 && cur->quantity > 0) {
                arr[idx++] = cur;
            }
        }
    }
    *outArray = arr;
    return matchCount;
}

int inventoryGetAll(Medicine ***outArray)
{
    int total = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
            total++;
        }
    }
    if (total == 0) {
        *outArray = NULL;
        return 0;
    }

    Medicine **arr = malloc((size_t)total * sizeof(Medicine *));
    if (!arr) {
        *outArray = NULL;
        return 0;
    }

    int idx = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
            arr[idx++] = cur;
        }
    }
    *outArray = arr;
    return total;
}

OpStatus inventorySave(void)
{
    /* atomic write: build the whole file in a temp file, then rename()
     * over the real one so a crash mid-write can't corrupt existing data */
    FILE *fp = fopen(INVENTORY_TMP, "w");
    if (!fp) {
        return OP_FILE_ERROR;
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
            fprintf(fp, "%d|%s|%s|%d|%s|%d\n",
                    cur->id, cur->name, cur->batch, cur->quantity,
                    cur->expiryDate, cur->reorderLevel);
        }
    }
    fclose(fp);

    if (rename(INVENTORY_TMP, INVENTORY_FILE) != 0) {
        return OP_FILE_ERROR;
    }
    return OP_SUCCESS;
}

OpStatus inventoryAddNew(const char *name, const char *batch, int quantity,
                          const char *expiryDate, int reorderLevel,
                          const char *actor, int *outId)
{
    if (!name || name[0] == '\0' || !batch || batch[0] == '\0') {
        return OP_INVALID_INPUT;
    }
    if (containsDelimiter(name) || containsDelimiter(batch)) {
        return OP_INVALID_INPUT;
    }
    if (quantity < 0 || reorderLevel < 0) {
        return OP_INVALID_INPUT;
    }
    if (!isValidDate(expiryDate)) {
        return OP_INVALID_INPUT;
    }
    if (inventoryFindExact(name, batch, expiryDate) != NULL) {
        return OP_DUPLICATE;
    }

    Medicine *m = malloc(sizeof(Medicine));
    if (!m) {
        return OP_FILE_ERROR;
    }

    m->id = nextRecordId++;
    strncpy(m->name, name, MAX_NAME_LEN - 1);
    m->name[MAX_NAME_LEN - 1] = '\0';
    strncpy(m->batch, batch, MAX_BATCH_LEN - 1);
    m->batch[MAX_BATCH_LEN - 1] = '\0';
    m->quantity = quantity;
    strncpy(m->expiryDate, expiryDate, MAX_DATE_LEN - 1);
    m->expiryDate[MAX_DATE_LEN - 1] = '\0';
    m->reorderLevel = reorderLevel;
    m->next = NULL;

    insertNode(m);

    if (inventorySave() != OP_SUCCESS) {
        return OP_FILE_ERROR;
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "added record id=%d name=%s batch=%s qty=%d",
             m->id, m->name, m->batch, m->quantity);
    logEvent(actor, "ADD_INVENTORY", detail);

    if (outId) {
        *outId = m->id;
    }
    return OP_SUCCESS;
}

OpStatus inventoryRemove(int id, const char *actor)
{
    unsigned int slot = hashId(id);
    Medicine *cur = table[slot];
    Medicine *prev = NULL;

    while (cur) {
        if (cur->id == id) {
            if (prev) {
                prev->next = cur->next;
            } else {
                table[slot] = cur->next;
            }

            char detail[MAX_LINE_LEN];
            snprintf(detail, sizeof(detail), "removed record id=%d name=%s batch=%s",
                     cur->id, cur->name, cur->batch);

            free(cur);

            if (inventorySave() != OP_SUCCESS) {
                return OP_FILE_ERROR;
            }
            logEvent(actor, "REMOVE_INVENTORY", detail);
            return OP_SUCCESS;
        }
        prev = cur;
        cur = cur->next;
    }
    return OP_NOT_FOUND;
}

OpStatus inventoryUpdate(int id, const char *name, const char *batch,
                          int reorderLevel, const char *actor)
{
    Medicine *m = inventoryFindById(id);
    if (!m) {
        return OP_NOT_FOUND;
    }
    if (!name || name[0] == '\0' || !batch || batch[0] == '\0' || reorderLevel < 0) {
        return OP_INVALID_INPUT;
    }
    if (containsDelimiter(name) || containsDelimiter(batch)) {
        return OP_INVALID_INPUT;
    }

    strncpy(m->name, name, MAX_NAME_LEN - 1);
    m->name[MAX_NAME_LEN - 1] = '\0';
    strncpy(m->batch, batch, MAX_BATCH_LEN - 1);
    m->batch[MAX_BATCH_LEN - 1] = '\0';
    m->reorderLevel = reorderLevel;

    if (inventorySave() != OP_SUCCESS) {
        return OP_FILE_ERROR;
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "updated record id=%d name=%s batch=%s reorder=%d",
             m->id, m->name, m->batch, m->reorderLevel);
    logEvent(actor, "UPDATE_INVENTORY", detail);
    return OP_SUCCESS;
}

OpStatus inventoryIncreaseStock(int id, int qty, const char *actor)
{
    if (qty <= 0) {
        return OP_INVALID_INPUT;
    }
    Medicine *m = inventoryFindById(id);
    if (!m) {
        return OP_NOT_FOUND;
    }

    m->quantity += qty;

    if (inventorySave() != OP_SUCCESS) {
        return OP_FILE_ERROR;
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "stock increase id=%d name=%s qty=+%d new_total=%d",
             m->id, m->name, qty, m->quantity);
    logEvent(actor, "STOCK_INCREASE", detail);
    return OP_SUCCESS;
}

OpStatus inventoryDecreaseStock(int id, int qty, const char *actor)
{
    if (qty <= 0) {
        return OP_INVALID_INPUT;
    }
    Medicine *m = inventoryFindById(id);
    if (!m) {
        return OP_NOT_FOUND;
    }
    if (qty > m->quantity) {
        return OP_INSUFFICIENT_STOCK;
    }

    m->quantity -= qty;

    if (inventorySave() != OP_SUCCESS) {
        return OP_FILE_ERROR;
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "stock decrease id=%d name=%s qty=-%d new_total=%d",
             m->id, m->name, qty, m->quantity);
    logEvent(actor, "STOCK_DECREASE", detail);
    return OP_SUCCESS;
}
