#include <pthread.h>

#include "concurrency.h"

static pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

void dataLock(void)
{
    pthread_mutex_lock(&dataMutex);
}

void dataUnlock(void)
{
    pthread_mutex_unlock(&dataMutex);
}
