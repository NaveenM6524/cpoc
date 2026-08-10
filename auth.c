#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth.h"
#include "util.h"
#include "logging.h"

static User *users = NULL;
static int userCount = 0;
static int userCapacity = 0;

/* FNV-1a hashing - NOT cryptographically secure, just avoids storing
 * plaintext passwords on disk for this training project */
static unsigned long fnv1aHash(const char *str)
{
    unsigned long hash = 2166136261UL;
    while (*str) {
        hash ^= (unsigned char)(*str++);
        hash *= 16777619UL;
    }
    return hash;
}

static void hashPassword(const char *password, char *outHash)
{
    unsigned long h = fnv1aHash(password);
    snprintf(outHash, MAX_HASH_LEN, "%016lx", h);
}

static void growIfNeeded(void)
{
    if (userCount < userCapacity) {
        return;
    }
    int newCapacity = (userCapacity == 0) ? 8 : userCapacity * 2;
    User *grown = realloc(users, (size_t)newCapacity * sizeof(User));
    if (!grown) {
        return;
    }
    users = grown;
    userCapacity = newCapacity;
}

static User *findUser(const char *username)
{
    for (int i = 0; i < userCount; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

static OpStatus saveUsers(void)
{
    FILE *fp = fopen(USERS_TMP, "w");
    if (!fp) {
        return OP_FILE_ERROR;
    }
    for (int i = 0; i < userCount; i++) {
        fprintf(fp, "%s|%s|%d|%d|%d\n",
                users[i].username, users[i].passwordHash, users[i].isAdmin,
                users[i].failedAttempts, users[i].locked);
    }
    fclose(fp);

    if (rename(USERS_TMP, USERS_FILE) != 0) {
        return OP_FILE_ERROR;
    }
    return OP_SUCCESS;
}

void authInit(void)
{
    ensureDataDir();
    userCount = 0;

    FILE *fp = fopen(USERS_FILE, "r");
    if (fp) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            if (line[0] == '\0') {
                continue;
            }

            char username[MAX_USERNAME_LEN], hash[MAX_HASH_LEN];
            int isAdmin, failedAttempts, locked;

            int fields = sscanf(line, "%31[^|]|%64[^|]|%d|%d|%d",
                                 username, hash, &isAdmin, &failedAttempts, &locked);

            if (fields != 5) {
                logEvent("SYSTEM", "LOAD_WARNING", "corrupt user row skipped");
                continue;
            }

            growIfNeeded();
            if (userCount >= userCapacity) {
                continue; /* allocation failed, drop the row rather than crash */
            }

            User *u = &users[userCount++];
            strncpy(u->username, username, MAX_USERNAME_LEN - 1);
            u->username[MAX_USERNAME_LEN - 1] = '\0';
            strncpy(u->passwordHash, hash, MAX_HASH_LEN - 1);
            u->passwordHash[MAX_HASH_LEN - 1] = '\0';
            u->isAdmin = isAdmin;
            u->failedAttempts = failedAttempts;
            u->locked = locked;
        }
        fclose(fp);
    }

    if (userCount == 0) {
        /* first run - seed a default admin so the system is reachable */
        growIfNeeded();
        User *u = &users[userCount++];
        strncpy(u->username, "admin", MAX_USERNAME_LEN - 1);
        u->username[MAX_USERNAME_LEN - 1] = '\0';
        hashPassword("admin123", u->passwordHash);
        u->isAdmin = 1;
        u->failedAttempts = 0;
        u->locked = 0;
        saveUsers();
        logEvent("SYSTEM", "INIT", "default admin account created (admin/admin123)");
        printf("No users found - created default account admin / admin123\n");
        printf("Please change this after logging in by creating a new admin user.\n");
    }
}

void authFreeAll(void)
{
    free(users);
    users = NULL;
    userCount = 0;
    userCapacity = 0;
}

OpStatus authLogin(const char *username, const char *password, User *outUser)
{
    User *u = findUser(username);
    if (!u) {
        return OP_NOT_FOUND;
    }

    if (u->locked) {
        if (outUser) *outUser = *u;
        logEvent(username, "LOGIN_BLOCKED", "attempt on locked account");
        return OP_INVALID_INPUT;
    }

    char hash[MAX_HASH_LEN];
    hashPassword(password, hash);

    if (strcmp(hash, u->passwordHash) == 0) {
        u->failedAttempts = 0;
        saveUsers();
        if (outUser) *outUser = *u;
        logEvent(username, "LOGIN_SUCCESS", "login ok");
        return OP_SUCCESS;
    }

    u->failedAttempts++;
    if (u->failedAttempts >= MAX_LOGIN_ATTEMPTS) {
        u->locked = 1;
        logEvent(username, "ACCOUNT_LOCKED", "too many failed attempts");
    }
    saveUsers();
    if (outUser) *outUser = *u;
    logEvent(username, "LOGIN_FAILED", "wrong password");
    return OP_INVALID_INPUT;
}

OpStatus authCreateUser(const char *username, const char *password, int isAdmin,
                         const char *creatorUsername)
{
    if (!username || username[0] == '\0' || !password || password[0] == '\0') {
        return OP_INVALID_INPUT;
    }
    if (containsDelimiter(username)) {
        return OP_INVALID_INPUT;
    }
    if (findUser(username) != NULL) {
        return OP_DUPLICATE;
    }

    growIfNeeded();
    if (userCount >= userCapacity) {
        return OP_FILE_ERROR;
    }

    User *u = &users[userCount++];
    strncpy(u->username, username, MAX_USERNAME_LEN - 1);
    u->username[MAX_USERNAME_LEN - 1] = '\0';
    hashPassword(password, u->passwordHash);
    u->isAdmin = isAdmin;
    u->failedAttempts = 0;
    u->locked = 0;

    if (saveUsers() != OP_SUCCESS) {
        userCount--;
        return OP_FILE_ERROR;
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "created user=%s role=%s",
             username, isAdmin ? "admin" : "staff");
    logEvent(creatorUsername, "CREATE_USER", detail);
    return OP_SUCCESS;
}
