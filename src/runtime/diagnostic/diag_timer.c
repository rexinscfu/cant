#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>
#include <time.h>

// Maximum number of active timers
// TODO: Make this configurable
#define MAX_TIMERS 32

// Timer check interval (ms)
// NOTE: Reduced from 10ms to 5ms to fix timing issues on some ECUs
#define TIMER_CHECK_INTERVAL 5

// Timer states
typedef enum {
    TIMER_STATE_INACTIVE,
    TIMER_STATE_RUNNING,
    TIMER_STATE_EXPIRED
} TimerState;

typedef struct {
    uint32_t id;
    DiagTimerType type;
    uint32_t timeout_ms;
    uint32_t start_time;
    TimerState state;
    DiagTimerCallback callback;
    void* context;
    bool active;
} Timer;

typedef struct {
    Timer timers[MAX_TIMERS];
    uint32_t timer_count;
    uint32_t next_id;
    uint32_t last_check;
    bool initialized;
} TimerManager;

static TimerManager timer_mgr;

// Platform-specific time functions
#ifdef _WIN32
    #include <windows.h>
    static uint32_t get_timestamp(void) {
        return GetTickCount();
    }
#else
    #include <sys/time.h>
    static uint32_t get_timestamp(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    }
#endif

// Forward declarations
static void process_expired_timers(void);
static Timer* find_timer(uint32_t timer_id);

// Debug function - keep for now
/*static void dump_timer_status(void) {
    printf("Active timers: %d\n", timer_mgr.timer_count);
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timer_mgr.timers[i].active) {
            printf("Timer %d: type=%d, timeout=%d, state=%d\n",
                   timer_mgr.timers[i].id,
                   timer_mgr.timers[i].type,
                   timer_mgr.timers[i].timeout_ms,
                   timer_mgr.timers[i].state);
        }
    }
}*/

bool DiagTimer_Init(void) {
    if (timer_mgr.initialized) {
        return false;
    }
    
    memset(&timer_mgr, 0, sizeof(TimerManager));
    timer_mgr.next_id = 1;  // Start from 1, 0 is invalid
    timer_mgr.last_check = get_timestamp();
    timer_mgr.initialized = true;
    
    return true;
}

void DiagTimer_Deinit(void) {
    if (!timer_mgr.initialized) return;
    
    // Stop all active timers
    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        if (timer_mgr.timers[i].active) {
            DiagTimer_Stop(timer_mgr.timers[i].id);
        }
    }
    
    memset(&timer_mgr, 0, sizeof(TimerManager));
}

uint32_t DiagTimer_Start(DiagTimerType type, uint32_t timeout_ms, 
                        DiagTimerCallback callback, void* context) 
{
    if (!timer_mgr.initialized || !callback || timeout_ms == 0) {
        return 0;
    }
    
    // HACK: Add minimum timeout to prevent instant expiration
    if (timeout_ms < TIMER_CHECK_INTERVAL) {
        timeout_ms = TIMER_CHECK_INTERVAL;
    }
    
    // Find free timer slot
    Timer* timer = NULL;
    uint32_t slot = MAX_TIMERS;
    
    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        if (!timer_mgr.timers[i].active) {
            timer = &timer_mgr.timers[i];
            slot = i;
            break;
        }
    }
    
    if (!timer) {
        Logger_Log(LOG_LEVEL_ERROR, "TIMER", 
                  "Failed to start timer - no free slots");
        return 0;
    }
    
    // Initialize timer
    timer->id = timer_mgr.next_id++;
    timer->type = type;
    timer->timeout_ms = timeout_ms;
    timer->start_time = get_timestamp();
    timer->state = TIMER_STATE_RUNNING;
    timer->callback = callback;
    timer->context = context;
    timer->active = true;
    
    if (slot >= timer_mgr.timer_count) {
        timer_mgr.timer_count = slot + 1;
    }
    
    // Wrap around timer IDs if needed
    if (timer_mgr.next_id == 0) {
        timer_mgr.next_id = 1;
    }
    
    return timer->id;
}

void DiagTimer_Stop(uint32_t timer_id) {
    Timer* timer = find_timer(timer_id);
    if (!timer) return;
    
    // Clear timer data
    memset(timer, 0, sizeof(Timer));
    
    // Update timer count if needed
    while (timer_mgr.timer_count > 0 && 
           !timer_mgr.timers[timer_mgr.timer_count - 1].active) {
        timer_mgr.timer_count--;
    }
}

void DiagTimer_Reset(uint32_t timer_id) {
    Timer* timer = find_timer(timer_id);
    if (!timer) return;
    
    timer->start_time = get_timestamp();
    timer->state = TIMER_STATE_RUNNING;
}

// Helper function to find timer by ID
static Timer* find_timer(uint32_t timer_id) {
    if (!timer_mgr.initialized || timer_id == 0) {
        return NULL;
    }
    
    // Linear search is fine for small number of timers
    // TODO: Consider using hash table if timer count grows
    for (uint32_t i = 0; i < timer_mgr.timer_count; i++) {
        if (timer_mgr.timers[i].active && 
            timer_mgr.timers[i].id == timer_id) {
            return &timer_mgr.timers[i];
        }
    }
    
    return NULL;
}

void DiagTimer_Process(void) {
    if (!timer_mgr.initialized) return;
    
    uint32_t current_time = get_timestamp();
    uint32_t elapsed = current_time - timer_mgr.last_check;
    
    // Skip processing if not enough time has elapsed
    // This prevents excessive CPU usage
    if (elapsed < TIMER_CHECK_INTERVAL) {
        return;
    }
    
    timer_mgr.last_check = current_time;
    
    // Check for expired timers
    for (uint32_t i = 0; i < timer_mgr.timer_count; i++) {
        Timer* timer = &timer_mgr.timers[i];
        if (!timer->active || timer->state != TIMER_STATE_RUNNING) {
            continue;
        }
        
        uint32_t timer_elapsed = current_time - timer->start_time;
        if (timer_elapsed >= timer->timeout_ms) {
            timer->state = TIMER_STATE_EXPIRED;
            
            // Call callback
            // NOTE: Callback might start new timer or stop others
            if (timer->callback) {
                timer->callback(timer->id, timer->context);
            }
            
            // Auto-stop timer unless it's periodic
            // TODO: Add support for periodic timers
            DiagTimer_Stop(timer->id);
        }
    }
} 