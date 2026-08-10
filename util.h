#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include "common.h"

/* creates the data/ folder if it does not already exist */
void ensureDataDir(void);

/* accepts flexible input like "2-5-2026" or "02-5-2026" and writes the
 * zero-padded "DD-MM-YYYY" form into output (must be >= MAX_DATE_LEN bytes).
 * returns 1 on a parseable format, 0 otherwise. Does NOT check calendar
 * validity - call isValidDate() on the normalized result for that. */
int normalizeDate(const char *input, char *output);

/* strict calendar validation on an already-normalized "DD-MM-YYYY" string */
int isValidDate(const char *ddmmyyyy);

int isLeapYear(int year);
int daysInMonth(int month, int year);

/* Julian Day Number conversion, used for accurate day-difference math
 * across month/year boundaries and leap years */
long dateToJDN(int day, int month, int year);

/* compares two normalized "DD-MM-YYYY" dates via JDN.
 * returns <0 if dateA earlier, 0 if equal, >0 if dateA later */
int compareDates(const char *dateA, const char *dateB);

void getCurrentTimestamp(char *buffer, size_t size);
void getCurrentDate(char *buffer, size_t size);

/* returns 1 if str contains '|' - the field delimiter used by every
 * .dat file. Any free-text field written to a data file must be
 * checked with this first, or it silently corrupts that row's field
 * alignment (and the row gets skipped as corrupt on the next load). */
int containsDelimiter(const char *str);

#endif
