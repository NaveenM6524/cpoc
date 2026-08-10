#ifndef REPORTS_H
#define REPORTS_H

#include <stdio.h>

void reportStock(FILE *out, const char *viewer);
void reportLowStock(FILE *out, const char *viewer);
void reportExpiry(FILE *out, int daysWindow, const char *viewer);
void reportSupplyHistory(FILE *out, const char *viewer);
void reportDistributionHistory(FILE *out, const char *viewer);
void reportAccountability(FILE *out, const char *viewer);

#endif
