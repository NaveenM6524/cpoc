#ifndef INVENTORY_H
#define INVENTORY_H

#include "common.h"

void inventoryInit(void);
void inventoryFreeAll(void);

OpStatus inventoryAddNew(const char *name, const char *batch, int quantity,
                          const char *expiryDate, int reorderLevel,
                          const char *actor, int *outId);

OpStatus inventoryRemove(int id, const char *actor);

OpStatus inventoryUpdate(int id, const char *name, const char *batch,
                          int reorderLevel, const char *actor);

OpStatus inventoryIncreaseStock(int id, int qty, const char *actor);
OpStatus inventoryDecreaseStock(int id, int qty, const char *actor);

Medicine *inventoryFindById(int id);
Medicine *inventoryFindExact(const char *name, const char *batch, const char *expiryDate);

/* fills *outArray with a freshly malloc'd array of pointers to every
 * live batch matching name (quantity > 0); returns the count.
 * caller must free(*outArray) — the Medicine records themselves are
 * NOT owned by the caller, only the array of pointers is. */
int inventoryFindAllByName(const char *name, Medicine ***outArray);

/* same allocation contract as inventoryFindAllByName, but returns
 * every live record regardless of quantity */
int inventoryGetAll(Medicine ***outArray);

OpStatus inventorySave(void);

#endif
