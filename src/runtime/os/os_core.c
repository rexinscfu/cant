#include "os_core.h"
#include <string.h>
#include "os_internal.h"
#include "../utils/critical.h"

// Internal OS state
static struct {
    AppModeType active_mode;
    TaskType current_task;
    TaskType highest_ready;
    uint8_t interrupt_level;
    bool os_started;
    
    struct {
        TaskConfigType* configs;
        TaskStateType* states;
        uint32_t count;
    } tasks;
    
    struct {
        ResourceConfigType* configs;
        TaskType* owners;
        uint32_t count;
    } resources;
    
    struct {
        AlarmConfigType* configs;
        uint32_t* values;
        bool* active;
        uint32_t count;
    } alarms;
    
    struct {
        CounterConfigType* configs;
        uint32_t* values;
        uint32_t count;
    } counters;
    
    CriticalSection critical;
} os_state;

// Internal functions
static void os_scheduler(void) {
    TaskType highest_prio_task = INVALID_TASK;
    TaskPrioType highest_prio = 0;
    
    // Find highest priority ready task
    for (uint32_t i = 0; i < os_state.tasks.count; i++) {
        if (os_state.tasks.states[i] == READY &&
            os_state.tasks.configs[i].priority > highest_prio) {
            highest_prio = os_state.tasks.configs[i].priority;
            highest_prio_task = i;
        }
    }
    
    os_state.highest_ready = highest_prio_task;
    
    // Perform context switch if needed
    if (os_state.highest_ready != os_state.current_task) {
        // Save context of current task
        if (os_state.current_task != INVALID_TASK) {
            // Platform-specific context save
        }
        
        os_state.current_task = os_state.highest_ready;
        
        // Restore context of new task
        if (os_state.current_task != INVALID_TASK) {
            // Platform-specific context restore
        }
    }
}

static void os_tick_handler(void) {
    enter_critical(&os_state.critical);
    
    // Update counters and alarms
    for (uint32_t i = 0; i < os_state.counters.count; i++) {
        os_state.counters.values[i]++;
        if (os_state.counters.values[i] > os_state.counters.configs[i].max_allowed_value) {
            os_state.counters.values[i] = 0;
        }
        
        // Process alarms linked to this counter
        for (uint32_t j = 0; j < os_state.alarms.count; j++) {
            if (os_state.alarms.configs[j].counter_id == i && os_state.alarms.active[j]) {
                if (--os_state.alarms.values[j] == 0) {
                    // Alarm expired
                    if (os_state.alarms.configs[j].action) {
                        os_state.alarms.configs[j].action();
                    }
                    if (os_state.alarms.configs[j].task_id != INVALID_TASK) {
                        ActivateTask(os_state.alarms.configs[j].task_id);
                    }
                    if (os_state.alarms.configs[j].event != 0) {
                        SetEvent(os_state.alarms.configs[j].task_id, 
                                os_state.alarms.configs[j].event);
                    }
                    
                    // Handle cyclic alarms
                    if (os_state.alarms.configs[j].cycle_time > 0) {
                        os_state.alarms.values[j] = os_state.alarms.configs[j].cycle_time;
                    } else {
                        os_state.alarms.active[j] = false;
                    }
                }
            }
        }
    }
    
    os_scheduler();
    exit_critical(&os_state.critical);
}

// OS API Implementation
StatusType StartOS(AppModeType mode) {
    if (os_state.os_started) {
        return E_OS_STATE;
    }
    
    enter_critical(&os_state.critical);
    
    os_state.active_mode = mode;
    os_state.current_task = INVALID_TASK;
    os_state.highest_ready = INVALID_TASK;
    
    // Initialize tasks
    for (uint32_t i = 0; i < os_state.tasks.count; i++) {
        if (os_state.tasks.configs[i].is_autostart) {
            os_state.tasks.states[i] = READY;
        } else {
            os_state.tasks.states[i] = SUSPENDED;
        }
    }
    
    // Initialize resources
    for (uint32_t i = 0; i < os_state.resources.count; i++) {
        os_state.resources.owners[i] = INVALID_TASK;
    }
    
    // Initialize alarms
    for (uint32_t i = 0; i < os_state.alarms.count; i++) {
        os_state.alarms.active[i] = false;
        os_state.alarms.values[i] = 0;
    }
    
    // Initialize counters
    for (uint32_t i = 0; i < os_state.counters.count; i++) {
        os_state.counters.values[i] = 0;
    }
    
    os_state.os_started = true;
    
    // Start system timer
    // Platform-specific timer initialization
    
    exit_critical(&os_state.critical);
    
    os_scheduler();
    return E_OK;
}

StatusType ActivateTask(TaskType task_id) {
    if (task_id >= os_state.tasks.count) {
        return E_OS_ID;
    }
    
    enter_critical(&os_state.critical);
    
    if (os_state.tasks.states[task_id] == SUSPENDED) {
        os_state.tasks.states[task_id] = READY;
        os_scheduler();
    }
    
    exit_critical(&os_state.critical);
    return E_OK;
}

