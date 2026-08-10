#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/msms.sock"

static int socketFd;

/* copies everything the server sends straight to our terminal, on its
 * own thread, so it can print prompts as they arrive while the main
 * thread is free to block waiting for the person to type */
static void *readerThread(void *arg)
{
    (void)arg;
    char buf[512];
    ssize_t n;
    while ((n = read(socketFd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, (size_t)n, stdout);
        fflush(stdout);
    }
    return NULL;
}

int main(void)
{
    socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(socketFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect (is the MSMS server running?)");
        close(socketFd);
        return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, readerThread, NULL);

    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (write(socketFd, line, strlen(line)) < 0) {
            break;
        }
    }

    shutdown(socketFd, SHUT_WR);
    pthread_join(tid, NULL);
    close(socketFd);
    return 0;
}
