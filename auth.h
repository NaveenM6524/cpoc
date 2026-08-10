#ifndef AUTH_H
#define AUTH_H

#include "common.h"

void authInit(void);
void authFreeAll(void);

/* returns OP_SUCCESS on a correct, unlocked login (outUser filled).
 * returns OP_NOT_FOUND if no such username exists.
 * returns OP_INVALID_INPUT for a wrong password or a locked account -
 * check outUser->locked to tell those two apart. */
OpStatus authLogin(const char *username, const char *password, User *outUser);

/* admin-only user creation (still an authentication concern, so it lives
 * here rather than in a separate module). */
OpStatus authCreateUser(const char *username, const char *password, int isAdmin,
                         const char *creatorUsername);

#endif
