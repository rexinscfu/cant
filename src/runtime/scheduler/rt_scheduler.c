#include "rt_scheduler.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include "../memory/rt_memory.h"
#include "../watchdog/watchdog.h"

#define MAX_TASKS 32
#define NSEC_PER_SEC 1000000000
#define NSEC_PER_USEC 1000

typedef struct {
    TaskConfig config;
    TaskState state;
    TaskStats stats;
    pthread_t thread;
    struct timespec next_release;
    RTMemPool* stack_pool;
    uint32_t current_execution_start;
    bool is_active;
} TaskControlBlock;

static struct {
    TaskControlBlock tasks[MAX_TASKS];
    size_t task_count;
    pthread_mutex_t scheduler_lock;
    bool is_running;
    Watchdog* watchdog;
} scheduler;

static void update_task_stats(TaskControlBlock* tcb, uint32_t execution_time) {
    tcb->stats.activation_count++;
    
    if (execution_time > tcb->config.deadline_us) {
        tcb->stats.deadline_misses++;
    }
    
    if (execution_time < tcb->stats.execution_time_min || 
        tcb->stats.execution_time_min == 0) {
        tcb->stats.execution_time_min = execution_time;
    }
    
    if (execution_time > tcb->stats.execution_time_max) {
        tcb->stats.execution_time_max = execution_time;
    }
    
    // Exponential moving average for execution time
    tcb->stats.execution_time_avg = 
        (tcb->stats.execution_time_avg * 7 + execution_time) / 8;
}

static void* task_wrapper(void* arg) {
    TaskControlBlock* tcb = (TaskControlBlock*)arg;
    struct timespec start, end;
    
    // Set thread priority
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO) - tcb->config.priority;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    
    while (scheduler.is_running) {
        // Wait for next period
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, 
                       &tcb->next_release, NULL);
        
        // Get start time
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        // Execute task
        tcb->state = TASK_STATE_RUNNING;
        watchdog_pat(scheduler.watchdog);
        tcb->config.entry_point(tcb->config.arg);
        tcb->state = TASK_STATE_READY;
        
        // Get end time and update stats
        clock_gettime(CLOCK_MONOTONIC, &end);
        uint32_t execution_time = 
            (end.tv_sec - start.tv_sec) * 1000000 +
            (end.tv_nsec - start.tv_nsec) / 1000;
        
        update_task_stats(tcb, execution_time);
        
        // Calculate next release time
        tcb->next_release.tv_nsec += tcb->config.period_us * NSEC_PER_USEC;
        if (tcb->next_release.tv_nsec >= NSEC_PER_SEC) {
            tcb->next_release.tv_sec++;
            tcb->next_release.tv_nsec -= NSEC_PER_SEC;
        }
    }
    
    return NULL;
}

bool scheduler_init(void) {
    memset(&scheduler, 0, sizeof(scheduler));
    
    if (pthread_mutex_init(&scheduler.scheduler_lock, NULL) != 0) {
        return false;
    }
    
    scheduler.watchdog = watchdog_create(100); // 100ms timeout
    if (!scheduler.watchdog) {
        pthread_mutex_destroy(&scheduler.scheduler_lock);
        return false;
    }
    
    return true;
}

bool scheduler_create_task(const TaskConfig* config) {
    if (!config || scheduler.task_count >= MAX_TASKS) {
        return false;
    }
    
    pthread_mutex_lock(&scheduler.scheduler_lock);
    
    TaskControlBlock* tcb = &scheduler.tasks[scheduler.task_count];
    memcpy(&tcb->config, config, sizeof(TaskConfig));
    
    // Initialize task statistics
    memset(&tcb->stats, 0, sizeof(TaskStats));
    tcb->stats.execution_time_min = UINT32_MAX;
    
    // Allocate stack memory pool
    tcb->stack_pool = rt_mempool_create(8192, 1); // 8KB stack
    if (!tcb->stack_pool) {
        pthread_mutex_unlock(&scheduler.scheduler_lock);
        return false;
    }
    
    scheduler.task_count++;
    pthread_mutex_unlock(&scheduler.scheduler_lock);
    
    return true;
}

void scheduler_start(void) {
    pthread_mutex_lock(&scheduler.scheduler_lock);
    
    if (!scheduler.is_running) {
        scheduler.is_running = true;
        
        // Start all tasks
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        for (size_t i = 0; i < scheduler.task_count; i++) {
            TaskControlBlock* tcb = &scheduler.tasks[i];
            tcb->next_release = now;
            tcb->state = TASK_STATE_READY;
            
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setstack(&attr, 
                                rt_mempool_alloc(tcb->stack_pool),
                                8192);
            
            pthread_create(&tcb->thread, &attr, task_wrapper, tcb);
            pthread_attr_destroy(&attr);
        }
        
        watchdog_start(scheduler.watchdog);
    }
    
    pthread_mutex_unlock(&scheduler.scheduler_lock);
}

void scheduler_stop(void) {
    pthread_mutex_lock(&scheduler.scheduler_lock);
    
    if (scheduler.is_running) {
        scheduler.is_running = false;
        watchdog_stop(scheduler.watchdog);
        
        // Wait for all tasks to complete
        for (size_t i = 0; i < scheduler.task_count; i++) {
            pthread_join(scheduler.tasks[i].thread, NULL);
            rt_mempool_destroy(scheduler.tasks[i].stack_pool);
        }
    }
    
    pthread_mutex_unlock(&scheduler.scheduler_lock);
}

TaskStats scheduler_get_task_stats(const char* task_name) {
    TaskStats empty_stats = {0};
    
    if (!task_name) {
        return empty_stats;
    }
    
    pthread_mutex_lock(&scheduler.scheduler_lock);
    
    for (size_t i = 0; i < scheduler.task_count; i++) {
        if (strcmp(scheduler.tasks[i].config.name, task_name) == 0) {
            TaskStats stats = scheduler.tasks[i].stats;
            pthread_mutex_unlock(&scheduler.scheduler_lock);
            return stats;
        }
    }
    
    pthread_mutex_unlock(&scheduler.scheduler_lock);
    return empty_stats;
} 