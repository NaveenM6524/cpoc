#include <stdio.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "auth.h"
#include "inventory.h"
#include "supply.h"
#include "distribution.h"
#include "reports.h"
#include "concurrency.h"
#include "menu.h"

/* sentinel distinct from any real menu number or the -1 "garbage input"
 * result, so callers can tell "stream closed" apart from "typo" */
#define MENU_EOF (-99999)

static void printStatus(FILE *out, OpStatus status)
{
    switch (status) {
        case OP_SUCCESS:            fprintf(out, "Done.\n"); break;
        case OP_NOT_FOUND:          fprintf(out, "Not found.\n"); break;
        case OP_DUPLICATE:          fprintf(out, "That already exists.\n"); break;
        case OP_INVALID_INPUT:      fprintf(out, "Invalid input - nothing was changed.\n"); break;
        case OP_INSUFFICIENT_STOCK: fprintf(out, "Not enough stock for that operation.\n"); break;
        case OP_FILE_ERROR:         fprintf(out, "A file error occurred - nothing was changed.\n"); break;
    }
}

static void clearInputLine(FILE *in)
{
    int c;
    while ((c = fgetc(in)) != '\n' && c != EOF) {
        /* discard */
    }
}

static int readMenuChoice(FILE *in)
{
    int choice;
    int result = fscanf(in, "%d", &choice);

    if (result == EOF) {
        /* stream is closed/exhausted - not just a bad typo. Do NOT loop
         * on this: fscanf won't block waiting for more input that will
         * never come, so treating this like ordinary invalid input
         * would spin the caller's menu loop forever at 100% CPU. */
        return MENU_EOF;
    }
    if (result != 1) {
        clearInputLine(in);
        return -1;
    }
    clearInputLine(in);
    return choice;
}

static void doLogin(FILE *in, FILE *out, User *sessionUser, int *loggedIn)
{
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];

    fprintf(out, "Username: ");
    if (!fgets(username, sizeof(username), in)) {
        username[0] = '\0';
    } else {
        username[strcspn(username, "\n")] = '\0';
    }

    fprintf(out, "Password: ");
    if (!fgets(password, sizeof(password), in)) {
        password[0] = '\0';
    } else {
        password[strcspn(password, "\n")] = '\0';
    }

    User result;
    dataLock();
    OpStatus status = authLogin(username, password, &result);
    dataUnlock();

    if (status == OP_SUCCESS) {
        *sessionUser = result;
        *loggedIn = 1;
        fprintf(out, "Welcome, %s (%s).\n", result.username, result.isAdmin ? "admin" : "staff");
        return;
    }

    if (status == OP_NOT_FOUND) {
        fprintf(out, "No such user.\n");
    } else if (result.locked) {
        fprintf(out, "Account is locked. Contact an administrator.\n");
    } else {
        fprintf(out, "Invalid password. Attempts used: %d/%d\n", result.failedAttempts, MAX_LOGIN_ATTEMPTS);
    }
    *loggedIn = 0;
}

static void handleAddInventory(FILE *in, FILE *out, const char *actor)
{
    char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN], expiryRaw[64];
    int quantity, reorderLevel;

    fprintf(out, "Medicine/equipment name: ");
    if (!fgets(name, sizeof(name), in)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    fprintf(out, "Batch number: ");
    if (!fgets(batch, sizeof(batch), in)) {
        batch[0] = '\0';
    } else {
        batch[strcspn(batch, "\n")] = '\0';
    }

    fprintf(out, "Quantity: ");
    if (fscanf(in, "%d", &quantity) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine(in);

    fprintf(out, "Expiry date (D-M-YYYY): ");
    if (!fgets(expiryRaw, sizeof(expiryRaw), in)) {
        expiryRaw[0] = '\0';
    } else {
        expiryRaw[strcspn(expiryRaw, "\n")] = '\0';
    }

    fprintf(out, "Reorder level: ");
    if (fscanf(in, "%d", &reorderLevel) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine(in);

    char normalized[MAX_DATE_LEN];
    if (!normalizeDate(expiryRaw, normalized) || !isValidDate(normalized)) {
        fprintf(out, "Invalid expiry date - nothing was changed.\n");
        return;
    }

    int newId;
    dataLock();
    OpStatus status = inventoryAddNew(name, batch, quantity, normalized, reorderLevel, actor, &newId);
    dataUnlock();
    if (status == OP_SUCCESS) {
        fprintf(out, "Added as record id %d.\n", newId);
    } else {
        printStatus(out, status);
    }
}

static void handleRemoveInventory(FILE *in, FILE *out, const char *actor)
{
    int id;
    fprintf(out, "Record id to remove: ");
    if (fscanf(in, "%d", &id) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input.\n");
        return;
    }
    clearInputLine(in);

    dataLock();
    OpStatus status = inventoryRemove(id, actor);
    dataUnlock();
    printStatus(out, status);
}

static void handleUpdateInventory(FILE *in, FILE *out, const char *actor)
{
    int id, reorderLevel;
    char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN];

    fprintf(out, "Record id to update: ");
    if (fscanf(in, "%d", &id) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input.\n");
        return;
    }
    clearInputLine(in);

    fprintf(out, "New name: ");
    if (!fgets(name, sizeof(name), in)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    fprintf(out, "New batch: ");
    if (!fgets(batch, sizeof(batch), in)) {
        batch[0] = '\0';
    } else {
        batch[strcspn(batch, "\n")] = '\0';
    }

    fprintf(out, "New reorder level: ");
    if (fscanf(in, "%d", &reorderLevel) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine(in);

    dataLock();
    OpStatus status = inventoryUpdate(id, name, batch, reorderLevel, actor);
    dataUnlock();
    printStatus(out, status);
}

static void handleRecordSupply(FILE *in, FILE *out, const char *actor)
{
    char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN], expiryRaw[64], supplier[MAX_SUPPLIER_LEN];
    int quantity, reorderLevel;

    fprintf(out, "Medicine/equipment name: ");
    if (!fgets(name, sizeof(name), in)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    fprintf(out, "Batch number: ");
    if (!fgets(batch, sizeof(batch), in)) {
        batch[0] = '\0';
    } else {
        batch[strcspn(batch, "\n")] = '\0';
    }

    fprintf(out, "Quantity received: ");
    if (fscanf(in, "%d", &quantity) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine(in);

    fprintf(out, "Expiry date (D-M-YYYY): ");
    if (!fgets(expiryRaw, sizeof(expiryRaw), in)) {
        expiryRaw[0] = '\0';
    } else {
        expiryRaw[strcspn(expiryRaw, "\n")] = '\0';
    }

    fprintf(out, "Reorder level (used only if this is a new batch): ");
    if (fscanf(in, "%d", &reorderLevel) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine(in);

    fprintf(out, "Supplier name: ");
    if (!fgets(supplier, sizeof(supplier), in)) {
        supplier[0] = '\0';
    } else {
        supplier[strcspn(supplier, "\n")] = '\0';
    }

    dataLock();
    OpStatus status = supplyProcess(name, batch, quantity, expiryRaw, reorderLevel, supplier, actor);
    dataUnlock();
    printStatus(out, status);
}

static void handleDistributionRequest(FILE *in, FILE *out, const char *actor)
{
    char name[MAX_NAME_LEN];
    int quantity;

    fprintf(out, "Medicine/equipment name: ");
    if (!fgets(name, sizeof(name), in)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    fprintf(out, "Quantity requested: ");
    if (fscanf(in, "%d", &quantity) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input.\n");
        return;
    }
    clearInputLine(in);

    SupplyRequest result;
    dataLock();
    OpStatus status = distributionProcessRequest(name, quantity, actor, &result);
    dataUnlock();

    if (status != OP_SUCCESS) {
        printStatus(out, status);
        return;
    }

    const char *statusText = (result.status == REQ_FULFILLED) ? "FULFILLED" :
                              (result.status == REQ_PARTIAL) ? "PARTIAL" : "REJECTED";
    fprintf(out, "Request #%d: %s (%d of %d fulfilled)\n",
           result.requestId, statusText, result.fulfilledQty, result.requestedQty);
}

static void handleExpiryReport(FILE *in, FILE *out, const char *actor)
{
    int days;
    fprintf(out, "Show items expiring within how many days? ");
    if (fscanf(in, "%d", &days) != 1 || days < 0) {
        clearInputLine(in);
        fprintf(out, "Invalid input.\n");
        return;
    }
    clearInputLine(in);
    dataLock();
    reportExpiry(out, days, actor);
    dataUnlock();
}

static void handleCreateUser(FILE *in, FILE *out, const char *actor)
{
    char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
    int roleChoice;

    fprintf(out, "New username: ");
    if (!fgets(username, sizeof(username), in)) {
        username[0] = '\0';
    } else {
        username[strcspn(username, "\n")] = '\0';
    }

    fprintf(out, "New password: ");
    if (!fgets(password, sizeof(password), in)) {
        password[0] = '\0';
    } else {
        password[strcspn(password, "\n")] = '\0';
    }

    fprintf(out, "Role (1 = admin, 2 = staff): ");
    if (fscanf(in, "%d", &roleChoice) != 1) {
        clearInputLine(in);
        fprintf(out, "Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine(in);

    if (roleChoice != 1 && roleChoice != 2) {
        fprintf(out, "Invalid role - nothing was changed.\n");
        return;
    }

    dataLock();
    OpStatus status = authCreateUser(username, password, roleChoice == 1, actor);
    dataUnlock();
    printStatus(out, status);
}

static void printMenu(FILE *out, int isAdmin)
{
    fprintf(out, "\n===== Medical Supply Management System =====\n");
    fprintf(out, " 1. View stock report\n");
    fprintf(out, " 2. View low stock report\n");
    fprintf(out, " 3. View expiry report\n");
    fprintf(out, " 4. View supply history\n");
    fprintf(out, " 5. View distribution history\n");
    fprintf(out, " 6. Process a distribution request\n");
    if (isAdmin) {
        fprintf(out, " 7. Add inventory item\n");
        fprintf(out, " 8. Remove inventory item\n");
        fprintf(out, " 9. Update inventory item\n");
        fprintf(out, "10. Record supply received\n");
        fprintf(out, "11. Create new user\n");
        fprintf(out, "12. View accountability report\n");
    }
    fprintf(out, " 0. Logout\n");
    fprintf(out, "Choice: ");
}

/* runs the post-login menu for one user until they log out.
 * returns 1 if the user wants to log in again as someone else,
 * 0 if they want to exit the session. */
static int runSession(FILE *in, FILE *out, const User *sessionUser)
{
    int sessionActive = 1;
    int streamClosed = 0;

    while (sessionActive) {
        printMenu(out, sessionUser->isAdmin);
        fflush(out);
        int choice = readMenuChoice(in);

        if (choice == MENU_EOF) {
            fprintf(out, "\nInput stream closed - logging out.\n");
            streamClosed = 1;
            break;
        }

        switch (choice) {
            case 1:
                dataLock(); reportStock(out, sessionUser->username); dataUnlock();
                break;
            case 2:
                dataLock(); reportLowStock(out, sessionUser->username); dataUnlock();
                break;
            case 3: handleExpiryReport(in, out, sessionUser->username); break;
            case 4:
                dataLock(); reportSupplyHistory(out, sessionUser->username); dataUnlock();
                break;
            case 5:
                dataLock(); reportDistributionHistory(out, sessionUser->username); dataUnlock();
                break;
            case 6: handleDistributionRequest(in, out, sessionUser->username); break;
            case 7:
                if (sessionUser->isAdmin) handleAddInventory(in, out, sessionUser->username);
                else fprintf(out, "Admins only.\n");
                break;
            case 8:
                if (sessionUser->isAdmin) handleRemoveInventory(in, out, sessionUser->username);
                else fprintf(out, "Admins only.\n");
                break;
            case 9:
                if (sessionUser->isAdmin) handleUpdateInventory(in, out, sessionUser->username);
                else fprintf(out, "Admins only.\n");
                break;
            case 10:
                if (sessionUser->isAdmin) handleRecordSupply(in, out, sessionUser->username);
                else fprintf(out, "Admins only.\n");
                break;
            case 11:
                if (sessionUser->isAdmin) handleCreateUser(in, out, sessionUser->username);
                else fprintf(out, "Admins only.\n");
                break;
            case 12:
                if (sessionUser->isAdmin) {
                    dataLock(); reportAccountability(out, sessionUser->username); dataUnlock();
                } else {
                    fprintf(out, "Admins only.\n");
                }
                break;
            case 0:
                sessionActive = 0;
                fprintf(out, "Logged out.\n");
                break;
            default:
                fprintf(out, "Unknown option.\n");
                break;
        }
        fflush(out);
    }

    if (streamClosed) {
        return 0;
    }

    fprintf(out, "Log in as another user? (1 = yes, 0 = exit program): ");
    fflush(out);
    return (readMenuChoice(in) == 1);
}

void runMenuSession(FILE *in, FILE *out)
{
    int running = 1;
    while (running) {
        User sessionUser;
        int loggedIn = 0;

        fprintf(out, "\n--- Login ---\n");
        fflush(out);
        while (!loggedIn) {
            doLogin(in, out, &sessionUser, &loggedIn);
            if (!loggedIn) {
                fprintf(out, "Try again? (1 = yes, 0 = exit program): ");
                fflush(out);
                int retry = readMenuChoice(in);
                if (retry != 1) {
                    running = 0;
                    break;
                }
            }
        }
        if (!loggedIn) {
            break;
        }

        running = runSession(in, out, &sessionUser);
    }

    fprintf(out, "Goodbye.\n");
    fflush(out);
}
