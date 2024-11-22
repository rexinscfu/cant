#include "critical.h"

#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION critical_section;
static bool initialized = false;

void enter_critical(void) {
    if (!initialized) {
        InitializeCriticalSection(&critical_section);
        initialized = true;
    }
    EnterCriticalSection(&critical_section);
}

void exit_critical(void) {
    if (initialized) {
        LeaveCriticalSection(&critical_section);
    }
}

#else
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void enter_critical(void) {
    pthread_mutex_lock(&mutex);
}

void exit_critical(void) {
    pthread_mutex_unlock(&mutex);
}
#endif 