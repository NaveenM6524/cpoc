#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include "common.h"

/* processes an outgoing supply request using FEFO (First-Expire-First-Out):
 * drains the earliest-expiring batch(es) first, spanning multiple batches
 * if one alone can't cover the request. Fills outResult with the exact
 * outcome (FULFILLED / PARTIAL / REJECTED) and quantity actually moved. */
OpStatus distributionProcessRequest(const char *medicineName, int requestedQty,
                                     const char *requestedBy, SupplyRequest *outResult);

#endif
