#include "timer.h"
#include "critical.h"
#include "../logging/diag_logger.h"
#include <string.h>
#include <time.h>

#define MAX_TIMERS 32

typedef struct {
    TimerConfig config;
    uint32_t next_trigger;
    bool active;
} TimerEntry;

typedef struct {
    TimerEntry timers[MAX_TIMERS];
    uint32_t timer_count;
    uint32_t start_time_ms;
    bool initialized;
} TimerManager;

static TimerManager timer_mgr;

#ifdef _WIN32
#include <windows.h>
static LARGE_INTEGER perf_freq;
static LARGE_INTEGER perf_start;
#else
#include <sys/time.h>
static struct timespec start_time;
#endif

bool Timer_Init(void) {
    memset(&timer_mgr, 0, sizeof(TimerManager));

#ifdef _WIN32
    if (!QueryPerformanceFrequency(&perf_freq)) {
        return false;
    }
    QueryPerformanceCounter(&perf_start);
#else
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

    timer_mgr.start_time_ms = 0;
    timer_mgr.initialized = true;

    Logger_Log(LOG_LEVEL_INFO, "TIMER", "Timer system initialized");
    return true;
}

void Timer_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "TIMER", "Timer system deinitialized");
    memset(&timer_mgr, 0, sizeof(TimerManager));
}

uint32_t Timer_GetMilliseconds(void) {
#ifdef _WIN32
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    return (uint32_t)((current.QuadPart - perf_start.QuadPart) * 1000 / perf_freq.QuadPart);
#else
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    return (uint32_t)((current.tv_sec - start_time.tv_sec) * 1000 +
                      (current.tv_nsec - start_time.tv_nsec) / 1000000);
#endif
}

uint32_t Timer_GetMicroseconds(void) {
#ifdef _WIN32
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    return (uint32_t)((current.QuadPart - perf_start.QuadPart) * 1000000 / perf_freq.QuadPart);
#else
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    return (uint32_t)((current.tv_sec - start_time.tv_sec) * 1000000 +
                      (current.tv_nsec - start_time.tv_nsec) / 1000);
#endif
}

void Timer_DelayMilliseconds(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

void Timer_DelayMicroseconds(uint32_t us) {
#ifdef _WIN32
    LARGE_INTEGER start, current, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    do {
        QueryPerformanceCounter(&current);
    } while ((current.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart < us);
#else
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
#endif
}

uint64_t Timer_GetHighResCounter(void) {
#ifdef _WIN32
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    return current.QuadPart;
#else
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    return (uint64_t)current.tv_sec * 1000000000ULL + current.tv_nsec;
#endif
}

uint64_t Timer_GetHighResFrequency(void) {
#ifdef _WIN32
    return perf_freq.QuadPart;
#else
    return 1000000000ULL; // Nanoseconds frequency
#endif
}

uint32_t Timer_CreateTimer(const TimerConfig* config) {
    if (!timer_mgr.initialized || !config || !config->callback) {
        return 0;
    }

    enter_critical();

    if (timer_mgr.timer_count >= MAX_TIMERS) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "TIMER", "Maximum number of timers reached");
        return 0;
    }

    // Find free slot
    uint32_t timer_id = 0;
    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        if (!timer_mgr.timers[i].active) {
            timer_id = i + 1; // Timer IDs start from 1
            TimerEntry* entry = &timer_mgr.timers[i];
            memcpy(&entry->config, config, sizeof(TimerConfig));
            entry->active = true;
            entry->next_trigger = Timer_GetMilliseconds() + config->period_ms;
            timer_mgr.timer_count++;
            break;
        }
    }

    exit_critical();
    
    if (timer_id) {
        Logger_Log(LOG_LEVEL_DEBUG, "TIMER", "Created timer ID %u, period %u ms",
                  timer_id, config->period_ms);
    }
    
    return timer_id;
}

bool Timer_StartTimer(uint32_t timer_id) {
    if (!timer_mgr.initialized || timer_id == 0 || timer_id > MAX_TIMERS) {
        return false;
    }

    enter_critical();
    
    TimerEntry* entry = &timer_mgr.timers[timer_id - 1];
    if (!entry->active) {
        exit_critical();
        return false;
    }

    entry->next_trigger = Timer_GetMilliseconds() + entry->config.period_ms;
    
    exit_critical();
    
    Logger_Log(LOG_LEVEL_DEBUG, "TIMER", "Started timer ID %u", timer_id);
    return true;
}

bool Timer_StopTimer(uint32_t timer_id) {
    if (!timer_mgr.initialized || timer_id == 0 || timer_id > MAX_TIMERS) {
        return false;
    }

    enter_critical();
    
    TimerEntry* entry = &timer_mgr.timers[timer_id - 1];
    if (!entry->active) {
        exit_critical();
        return false;
    }

    entry->next_trigger = 0;
    
    exit_critical();
    
    Logger_Log(LOG_LEVEL_DEBUG, "TIMER", "Stopped timer ID %u", timer_id);
    return true;
}

bool Timer_DeleteTimer(uint32_t timer_id) {
    if (!timer_mgr.initialized || timer_id == 0 || timer_id > MAX_TIMERS) {
        return false;
    }

    enter_critical();
    
    TimerEntry* entry = &timer_mgr.timers[timer_id - 1];
    if (!entry->active) {
        exit_critical();
        return false;
    }

    memset(entry, 0, sizeof(TimerEntry));
    timer_mgr.timer_count--;
    
    exit_critical();
    
    Logger_Log(LOG_LEVEL_DEBUG, "TIMER", "Deleted timer ID %u", timer_id);
    return true;
}

void Timer_Process(void) {
    if (!timer_mgr.initialized) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();

    enter_critical();

    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        TimerEntry* entry = &timer_mgr.timers[i];
        if (!entry->active || entry->next_trigger == 0) {
            continue;
        }

        if (current_time >= entry->next_trigger) {
            // Call the callback
            if (entry->config.callback) {
                entry->config.callback(entry->config.callback);
            }

            // Update next trigger time
            if (entry->config.repeat) {
                entry->next_trigger = current_time + entry->config.period_ms;
            } else {
                entry->next_trigger = 0;
            }
        }
    }

    exit_critical();
}

