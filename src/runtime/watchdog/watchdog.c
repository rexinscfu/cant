#include "watchdog.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

struct Watchdog {
    unsigned int timeout_ms;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    volatile bool running;
    volatile bool patted;
};

static void* watchdog_thread(void* arg) {
    Watchdog* watchdog = (Watchdog*)arg;
    struct timespec ts;
    
    while (watchdog->running) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += watchdog->timeout_ms * 1000000;
        
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += ts.tv_nsec / 1000000000;
            ts.tv_nsec %= 1000000000;
        }
        
        pthread_mutex_lock(&watchdog->lock);
        watchdog->patted = false;
        
        // Wait for pat or timeout
        pthread_cond_timedwait(&watchdog->cond, &watchdog->lock, &ts);
        
        if (!watchdog->patted) {
            // Watchdog timeout - handle system reset
            abort();  // In real system, trigger MCU reset
        }
        
        pthread_mutex_unlock(&watchdog->lock);
    }
    
    return NULL;
}

Watchdog* watchdog_create(unsigned int timeout_ms) {
    Watchdog* watchdog = malloc(sizeof(Watchdog));
    if (!watchdog) return NULL;
    
    watchdog->timeout_ms = timeout_ms;
    watchdog->running = false;
    watchdog->patted = false;
    
    pthread_mutex_init(&watchdog->lock, NULL);
    pthread_cond_init(&watchdog->cond, NULL);
    
    return watchdog;
}

void watchdog_start(Watchdog* watchdog) {
    if (!watchdog || watchdog->running) return;
    
    watchdog->running = true;
    pthread_create(&watchdog->thread, NULL, watchdog_thread, watchdog);
}

void watchdog_stop(Watchdog* watchdog) {
    if (!watchdog || !watchdog->running) return;
    
    watchdog->running = false;
    pthread_join(watchdog->thread, NULL);
}

void watchdog_pat(Watchdog* watchdog) {
    if (!watchdog) return;
    
    pthread_mutex_lock(&watchdog->lock);
    watchdog->patted = true;
    pthread_cond_signal(&watchdog->cond);
    pthread_mutex_unlock(&watchdog->lock);
} 