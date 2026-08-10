#ifndef CONCURRENCY_H
#define CONCURRENCY_H

/* MSMS keeps one in-memory copy of the truth (the inventory hash table
 * and the user list) shared by every client thread in the server.
 * dataLock()/dataUnlock() bracket every operation that touches that
 * shared state - one coarse-grained mutex, held for the whole
 * operation (not just a single function call), so a sequence like
 * "find exact batch, then increase its stock" in supply.c can never be
 * split across two different threads' operations. This directly closes
 * the lost-update race documented in HANDOFF.md, where two separate
 * processes each loaded their own snapshot and the last save silently
 * overwrote the other's write - with one shared process and one lock,
 * that class of bug can no longer happen. */
void dataLock(void);
void dataUnlock(void);

#endif
