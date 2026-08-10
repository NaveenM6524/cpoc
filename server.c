#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "util.h"
#include "auth.h"
#include "inventory.h"
#include "menu.h"

#define SOCKET_PATH "/tmp/msms.sock"
#define BACKLOG 16

static volatile sig_atomic_t serverRunning = 1;

static void handleShutdownSignal(int sig)
{
    (void)sig;
    serverRunning = 0;
}

/* tracks how many client threads are currently inside runMenuSession()
 * (i.e. might still touch the shared inventory/user data). Shutdown
 * must wait for this to reach zero before calling inventoryFreeAll()/
 * authFreeAll() - Helgrind caught exactly this race in testing: the
 * original shutdown path freed shared memory while a still-connected
 * client thread was concurrently reading it. */
static pthread_mutex_t clientCountMutex = PTHREAD_MUTEX_INITIALIZER;
static int activeClients = 0;

static void activeClientsIncrement(void)
{
    pthread_mutex_lock(&clientCountMutex);
    activeClients++;
    pthread_mutex_unlock(&clientCountMutex);
}

static void activeClientsDecrement(void)
{
    pthread_mutex_lock(&clientCountMutex);
    activeClients--;
    pthread_mutex_unlock(&clientCountMutex);
}

static int activeClientsGet(void)
{
    pthread_mutex_lock(&clientCountMutex);
    int n = activeClients;
    pthread_mutex_unlock(&clientCountMutex);
    return n;
}

typedef struct {
    int clientFd;
} ClientArgs;

/* one thread per connected client. Wraps the raw socket fd in two FILE
 * streams (one read, one write - a single fd is duplicated so buffered
 * stdio on each direction doesn't interfere with the other) and hands
 * them straight to the same runMenuSession() the standalone CLI uses,
 * so every menu, prompt, and validation rule behaves identically over
 * the network as it does locally. */
static void *clientThread(void *arg)
{
    ClientArgs *args = (ClientArgs *)arg;
    int fd = args->clientFd;
    free(args);

    int fdCopy = dup(fd);
    if (fdCopy < 0) {
        close(fd);
        return NULL;
    }

    FILE *in = fdopen(fd, "r");
    FILE *out = fdopen(fdCopy, "w");

    if (!in || !out) {
        fprintf(stderr, "warning: could not open client stream\n");
        if (in) fclose(in);
        if (out) fclose(out);
        if (!in) close(fd);
        if (!out) close(fdCopy);
        return NULL;
    }

    /* a socket is always fully-buffered by the C library (never
     * line-buffered the way a real terminal is), and most of our
     * prompts don't end in '\n' (e.g. "Username: "). Without this,
     * prompts sit in the buffer until something unrelated later
     * happens to flush it, so the user ends up typing blind, well
     * ahead of what they can actually see on screen. */
    setvbuf(out, NULL, _IONBF, 0);

    activeClientsIncrement();
    runMenuSession(in, out);
    activeClientsDecrement();

    fclose(in);
    fclose(out);
    return NULL;
}

int main(void)
{
    signal(SIGINT, handleShutdownSignal);
    signal(SIGTERM, handleShutdownSignal);

    ensureDataDir();
    inventoryInit();
    authInit();

    int serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH); /* remove a stale socket file from a previous run */

    if (bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, BACKLOG) < 0) {
        perror("listen");
        close(serverFd);
        return 1;
    }

    printf("MSMS server listening on %s (Ctrl+C to stop)\n", SOCKET_PATH);
    fflush(stdout);

    while (serverRunning) {
        int clientFd = accept(serverFd, NULL, NULL);
        if (clientFd < 0) {
            if (!serverRunning) {
                break; /* accept() interrupted by shutdown signal */
            }
            continue;
        }

        ClientArgs *args = malloc(sizeof(ClientArgs));
        if (!args) {
            close(clientFd);
            continue;
        }
        args->clientFd = clientFd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, clientThread, args) != 0) {
            fprintf(stderr, "warning: could not start a thread for a new client\n");
            free(args);
            close(clientFd);
            continue;
        }
        pthread_detach(tid);
    }

    close(serverFd);
    unlink(SOCKET_PATH);

    /* wait for every still-connected client thread to finish before
     * touching shared state - freeing it out from under an active
     * thread is exactly the race Helgrind caught during testing */
    printf("Waiting for %d active session(s) to finish...\n", activeClientsGet());
    fflush(stdout);
    while (activeClientsGet() > 0) {
        struct timespec delay = { 0, 50000000L }; /* 50ms */
        nanosleep(&delay, NULL);
    }

    inventoryFreeAll();
    authFreeAll();
    printf("\nServer shut down.\n");
    return 0;
}
