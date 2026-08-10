#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"

void ensureDataDir(void)
{
    struct stat st;
    if (stat(DATA_DIR, &st) == 0) {
        return; /* already exists */
    }
    if (mkdir(DATA_DIR, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "warning: could not create %s directory\n", DATA_DIR);
    }
}

int normalizeDate(const char *input, char *output)
{
    int day, month, year;

    if (sscanf(input, "%d-%d-%d", &day, &month, &year) != 3) {
        return 0;
    }
    if (day <= 0 || month <= 0 || year <= 0) {
        return 0;
    }

    snprintf(output, MAX_DATE_LEN, "%02d-%02d-%04d", day, month, year);
    return 1;
}

int isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int month, int year)
{
    static const int lengths[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return lengths[month - 1];
}

int isValidDate(const char *ddmmyyyy)
{
    int day, month, year;

    if (strlen(ddmmyyyy) != 10) {
        return 0;
    }
    if (sscanf(ddmmyyyy, "%2d-%2d-%4d", &day, &month, &year) != 3) {
        return 0;
    }
    if (year < 1900 || year > 2100) {
        return 0;
    }
    if (month < 1 || month > 12) {
        return 0;
    }
    if (day < 1 || day > daysInMonth(month, year)) {
        return 0;
    }
    return 1;
}

/* Julian Day Number - Fliegel & Van Flandern algorithm */
long dateToJDN(int day, int month, int year)
{
    long a = (14 - month) / 12;
    long y = year + 4800L - a;
    long m = month + 12 * a - 3;

    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

int compareDates(const char *dateA, const char *dateB)
{
    int dA, mA, yA, dB, mB, yB;

    sscanf(dateA, "%2d-%2d-%4d", &dA, &mA, &yA);
    sscanf(dateB, "%2d-%2d-%4d", &dB, &mB, &yB);

    long jdnA = dateToJDN(dA, mA, yA);
    long jdnB = dateToJDN(dB, mB, yB);

    if (jdnA < jdnB) return -1;
    if (jdnA > jdnB) return 1;
    return 0;
}

void getCurrentTimestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tmNow = localtime(&now);
    strftime(buffer, size, "%d-%m-%Y %H:%M:%S", tmNow);
}

void getCurrentDate(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tmNow = localtime(&now);
    strftime(buffer, size, "%d-%m-%Y", tmNow);
}

int containsDelimiter(const char *str)
{
    return strchr(str, '|') != NULL;
}
