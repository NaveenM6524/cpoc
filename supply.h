#ifndef SUPPLY_H
#define SUPPLY_H

#include "common.h"

/* records incoming stock (what arrived, how much, from whom, on what date)
 * and folds it into inventory.c's stock levels. expiryDateRaw may be in
 * unpadded form ("2-5-2026") and will be normalized internally. */
OpStatus supplyProcess(const char *medicineName, const char *batch, int quantity,
                        const char *expiryDateRaw, int reorderLevel,
                        const char *supplierName, const char *adminUsername);

#endif
