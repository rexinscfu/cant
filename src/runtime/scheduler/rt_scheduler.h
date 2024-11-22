#ifndef CANT_RT_SCHEDULER_H
#define CANT_RT_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Real-world task priorities (based on AUTOSAR OS)
typedef enum {
    TASK_PRIO_ISR2 = 0,      // Highest priority (interrupt level)
    TASK_PRIO_ALARM = 1,     // Alarm handling
    TASK_PRIO_ENGINE = 2,    // Engine control tasks
    TASK_PRIO_TRANS = 3,     // Transmission control
    TASK_PRIO_BRAKE = 4,     // Brake control
    TASK_PRIO_DIAG = 15,     // Diagnostic tasks (lowest)
} TaskPriority;

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_WAITING,
    TASK_STATE_SUSPENDED
} TaskState;

typedef struct {
    uint32_t period_us;      // Task period in microseconds
    uint32_t deadline_us;    // Relative deadline
    uint32_t wcet_us;        // Worst-case execution time
    TaskPriority priority;   // Task priority
    void (*entry_point)(void*);  // Task function
    void* arg;              // Task argument
    const char* name;       // Task name (for debugging)
} TaskConfig;

typedef struct {
    uint32_t deadline_misses;
    uint32_t execution_time_min;
    uint32_t execution_time_max;
    uint32_t execution_time_avg;
    uint32_t activation_count;
    uint32_t preemption_count;
} TaskStats;

// Task and scheduler API
bool scheduler_init(void);
bool scheduler_create_task(const TaskConfig* config);
void scheduler_start(void);
void scheduler_stop(void);
TaskStats scheduler_get_task_stats(const char* task_name);
void scheduler_reset_stats(void);

#endif // CANT_RT_SCHEDULER_H 