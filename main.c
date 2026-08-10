#include <stdio.h>

#include "util.h"
#include "auth.h"
#include "inventory.h"
#include "menu.h"

/* standalone single-user CLI entry point. All the actual menu logic
 * lives in menu.c so the multithreaded server (server.c) can reuse it
 * unchanged, one call per connected client. */
int main(void)
{
    /* see server.c's clientThread() for why: without this, prompts
     * that don't end in '\n' can sit unflushed until something else
     * happens to flush the stream, leaving the person typing blind. */
    setvbuf(stdout, NULL, _IONBF, 0);

    ensureDataDir();
    inventoryInit();
    authInit();

    runMenuSession(stdin, stdout);

    inventoryFreeAll();
    authFreeAll();
    return 0;
}
