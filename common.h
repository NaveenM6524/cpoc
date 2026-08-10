#ifndef COMMON_H
#define COMMON_H

/* ---- sizing constants ---- */
#define MAX_NAME_LEN      64
#define MAX_BATCH_LEN     32
#define MAX_USERNAME_LEN  32
#define MAX_PASSWORD_LEN  64
#define MAX_HASH_LEN      65
#define MAX_SUPPLIER_LEN  64
#define MAX_DATE_LEN      11   /* "DD-MM-YYYY" + '\0' */
#define MAX_TIMESTAMP_LEN 20   /* "DD-MM-YYYY HH:MM:SS" + '\0' */
#define MAX_LINE_LEN      512

#define HASH_TABLE_SIZE     101
#define MAX_LOGIN_ATTEMPTS  3

typedef enum {
    OP_SUCCESS,
    OP_NOT_FOUND,
    OP_DUPLICATE,
    OP_INVALID_INPUT,
    OP_INSUFFICIENT_STOCK,
    OP_FILE_ERROR
} OpStatus;

/* core inventory record - both medicines and equipment share this shape */
typedef struct Medicine {
    int id;
    char name[MAX_NAME_LEN];
    char batch[MAX_BATCH_LEN];
    int quantity;
    char expiryDate[MAX_DATE_LEN];
    int reorderLevel;
    struct Medicine *next;   /* separate chaining link */
} Medicine;

typedef struct {
    int id;
    char medicineName[MAX_NAME_LEN];
    int quantity;
    char supplierName[MAX_SUPPLIER_LEN];
    char addedByAdmin[MAX_USERNAME_LEN];
    char timestamp[MAX_TIMESTAMP_LEN];
} SupplyTransaction;

typedef enum {
    REQ_FULFILLED,
    REQ_PARTIAL,
    REQ_REJECTED
} RequestStatus;

typedef struct {
    int requestId;
    char medicineName[MAX_NAME_LEN];
    int requestedQty;
    int fulfilledQty;
    RequestStatus status;
    char timestamp[MAX_TIMESTAMP_LEN];
} SupplyRequest;

typedef struct {
    char username[MAX_USERNAME_LEN];
    char passwordHash[MAX_HASH_LEN];
    int isAdmin;
    int failedAttempts;
    int locked;
} User;

/* ---- data file locations (all inside data/) ---- */
#define DATA_DIR        "data"
#define INVENTORY_FILE  "data/inventory.dat"
#define INVENTORY_TMP   "data/inventory.dat.tmp"
#define USERS_FILE      "data/users.dat"
#define USERS_TMP       "data/users.dat.tmp"
#define SUPPLY_FILE     "data/supply.dat"
#define SUPPLY_TMP      "data/supply.dat.tmp"
#define REQUESTS_FILE   "data/requests.dat"
#define REQUESTS_TMP    "data/requests.dat.tmp"
#define AUDIT_LOG       "data/audit.log"

#endif
