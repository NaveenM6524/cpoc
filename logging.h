#ifndef LOGGING_H
#define LOGGING_H

/* appends one timestamped line to data/audit.log:
 * "DD-MM-YYYY HH:MM:SS | user | ACTION | detail" */
void logEvent(const char *user, const char *action, const char *detail);

#endif
