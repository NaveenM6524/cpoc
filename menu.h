#ifndef MENU_H
#define MENU_H

#include <stdio.h>

/* runs the full login+menu experience over any readable/writable stream
 * pair until the user exits or the stream closes. Used by:
 *   - main.c, with in=stdin, out=stdout (the original single-user CLI)
 *   - server.c, with in/out wrapping one client's socket, one call per
 *     connected client thread
 * Caller is responsible for calling ensureDataDir()/inventoryInit()/
 * authInit() once before the first call, and inventoryFreeAll()/
 * authFreeAll() once after the last one - this function does not own
 * that lifetime, since a server may run many sessions against the same
 * loaded data. */
void runMenuSession(FILE *in, FILE *out);

#endif
