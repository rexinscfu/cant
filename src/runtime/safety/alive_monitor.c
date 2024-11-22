#include "alive_monitor.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

#define MAX_TASKS 32

// Task monitoring state
typedef struct {
    Timer deadline_timer;
    Timer cycle_timer;
    uint32_t deadline_misses;
    uint32_t tolerance_count;
    uint32_t last_execution_time;
    AliveState state;
    bool first_execution;
} TaskMonitorState;

// Internal state
static struct {
    AliveMonitorConfig config;
    struct {
        TaskMonitorState tasks[MAX_TASKS];
        Timer supervision_timer;
        AliveState system_state;
        bool initialized;
    } state;
    CriticalSection critical;
} alive_monitor;

static bool validate_task_id(uint32_t task_id) {
    return task_id < alive_monitor.config.task_count;
}

static void update_system_state(void) {
    AliveState worst_state = ALIVE_STATE_HEALTHY;
    
    for (uint32_t i = 0; i < alive_monitor.config.task_count; i++) {
        const AliveTaskConfig* task_config = &alive_monitor.config.tasks[i];
        const TaskMonitorState* task_state = &alive_monitor.state.tasks[i];
        
        if (task_config->is_critical && task_state->state > worst_state) {
            worst_state = task_state->state;
        }
    }
    
    if (worst_state != alive_monitor.state.system_state) {
        alive_monitor.state.system_state = worst_state;
    }
}

bool Alive_Monitor_Init(const AliveMonitorConfig* config) {
    if (!config || !config->tasks || config->task_count == 0 || 
        config->task_count > MAX_TASKS) {
        return false;
    }
    
    enter_critical(&alive_monitor.critical);
    
    memcpy(&alive_monitor.config, config, sizeof(AliveMonitorConfig));
    memset(&alive_monitor.state, 0, sizeof(alive_monitor.state));
    
    // Initialize task states
    for (uint32_t i = 0; i < config->task_count; i++) {
        alive_monitor.state.tasks[i].state = ALIVE_STATE_HEALTHY;
        alive_monitor.state.tasks[i].first_execution = true;
    }
    
    timer_init();
    timer_start(&alive_monitor.state.supervision_timer, 
                config->supervision_interval_ms);
    
    alive_monitor.state.initialized = true;
    alive_monitor.state.system_state = ALIVE_STATE_HEALTHY;
    
    exit_critical(&alive_monitor.critical);
    return true;
}

void Alive_Monitor_DeInit(void) {
    enter_critical(&alive_monitor.critical);
    alive_monitor.state.initialized = false;
    memset(&alive_monitor.state, 0, sizeof(alive_monitor.state));
    exit_critical(&alive_monitor.critical);
}

void Alive_Monitor_Process(void) {
    if (!alive_monitor.state.initialized) return;
    
    enter_critical(&alive_monitor.critical);
    
    if (timer_expired(&alive_monitor.state.supervision_timer)) {
        // Check all tasks
        for (uint32_t i = 0; i < alive_monitor.config.task_count; i++) {
            TaskMonitorState* task_state = &alive_monitor.state.tasks[i];
            const AliveTaskConfig* task_config = &alive_monitor.config.tasks[i];
            
            if (!task_state->first_execution) {
                // Check deadline
                if (timer_expired(&task_state->deadline_timer)) {
                    task_state->deadline_misses++;
                    task_state->tolerance_count++;
                    
                    if (task_state->tolerance_count > task_config->tolerance_count) {
                        AliveState new_state = task_config->is_critical ? 
                            ALIVE_STATE_CRITICAL : ALIVE_STATE_DEGRADED;
                            
                        if (task_state->state != new_state) {
                            task_state->state = new_state;
                            if (alive_monitor.config.state_change_callback) {
                                alive_monitor.config.state_change_callback(i, new_state);
                            }
                        }
                    }
                }
            }
        }
        
        update_system_state();
        timer_start(&alive_monitor.state.supervision_timer, 
                   alive_monitor.config.supervision_interval_ms);
    }
    
    exit_critical(&alive_monitor.critical);
}

void Alive_Monitor_ReportAlive(uint32_t task_id) {
    if (!alive_monitor.state.initialized || !validate_task_id(task_id)) return;
    
    enter_critical(&alive_monitor.critical);
    
    TaskMonitorState* task_state = &alive_monitor.state.tasks[task_id];
    const AliveTaskConfig* task_config = &alive_monitor.config.tasks[task_id];
    
    uint32_t current_time = get_system_time_ms();
    
    if (!task_state->first_execution) {
        // Check cycle time
        uint32_t cycle_time = current_time - task_state->last_execution_time;
        
        if (cycle_time < task_config->min_cycle_ms || 
            cycle_time > task_config->max_cycle_ms) {
            task_state->tolerance_count++;
            
            if (task_state->tolerance_count > task_config->tolerance_count) {
                task_state->state = ALIVE_STATE_DEGRADED;
                if (alive_monitor.config.state_change_callback) {
                    alive_monitor.config.state_change_callback(task_id, 
                                                            ALIVE_STATE_DEGRADED);
                }
            }
            
            if (alive_monitor.config.error_callback) {
                alive_monitor.config.error_callback(task_id, cycle_time);
            }
        } else {
            // Reset tolerance if cycle time is good
            task_state->tolerance_count = 0;
            if (task_state->state != ALIVE_STATE_HEALTHY) {
                task_state->state = ALIVE_STATE_HEALTHY;
                if (alive_monitor.config.state_change_callback) {
                    alive_monitor.config.state_change_callback(task_id, 
                                                            ALIVE_STATE_HEALTHY);
                }
            }
        }
    }
    
    task_state->last_execution_time = current_time;
    task_state->first_execution = false;
    
    // Reset deadline timer
    timer_start(&task_state->deadline_timer, task_config->deadline_ms);
    
    update_system_state();
    
    exit_critical(&alive_monitor.critical);
}

AliveState Alive_Monitor_GetTaskState(uint32_t task_id) {
    if (!alive_monitor.state.initialized || !validate_task_id(task_id)) {
        return ALIVE_STATE_FAILED;
    }
    
    return alive_monitor.state.tasks[task_id].state;
}

AliveState Alive_Monitor_GetSystemState(void) {
    return alive_monitor.state.system_state;
}

uint32_t Alive_Monitor_GetDeadlineMisses(uint32_t task_id) {
    if (!alive_monitor.state.initialized || !validate_task_id(task_id)) {
        return 0;
    }
    
    return alive_monitor.state.tasks[task_id].deadline_misses;
}

void Alive_Monitor_ResetStatistics(void) {
    if (!alive_monitor.state.initialized) return;
    
    enter_critical(&alive_monitor.critical);
    
    for (uint32_t i = 0; i < alive_monitor.config.task_count; i++) {
        alive_monitor.state.tasks[i].deadline_misses = 0;
        alive_monitor.state.tasks[i].tolerance_count = 0;
        alive_monitor.state.tasks[i].state = ALIVE_STATE_HEALTHY;
    }
    
    alive_monitor.state.system_state = ALIVE_STATE_HEALTHY;
    
    exit_critical(&alive_monitor.critical);
}

bool Alive_Monitor_IsTaskHealthy(uint32_t task_id) {
    if (!alive_monitor.state.initialized || !validate_task_id(task_id)) {
        return false;
    }
    
    return alive_monitor.state.tasks[task_id].state == ALIVE_STATE_HEALTHY;
} 