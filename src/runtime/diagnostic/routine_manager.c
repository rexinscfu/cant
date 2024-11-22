#include "routine_manager.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"
#include "session_manager.h"

#define MAX_ROUTINES 32
#define MAX_ACTIVE_ROUTINES 8

// Routine Instance State
typedef struct {
    uint16_t routine_id;
    RoutineStatus status;
    Timer timeout_timer;
    uint16_t last_result_code;
} RoutineInstance;

// Internal state structure
typedef struct {
    RoutineManagerConfig config;
    RoutineDefinition routines[MAX_ROUTINES];
    RoutineInstance active_routines[MAX_ACTIVE_ROUTINES];
    uint32_t routine_count;
    uint32_t active_count;
    bool initialized;
    CriticalSection critical;
} RoutineManager;

static RoutineManager routine_manager;

static RoutineDefinition* find_routine(uint16_t routine_id) {
    for (uint32_t i = 0; i < routine_manager.routine_count; i++) {
        if (routine_manager.routines[i].routine_id == routine_id) {
            return &routine_manager.routines[i];
        }
    }
    return NULL;
}

static RoutineInstance* find_active_routine(uint16_t routine_id) {
    for (uint32_t i = 0; i < routine_manager.active_count; i++) {
        if (routine_manager.active_routines[i].routine_id == routine_id) {
            return &routine_manager.active_routines[i];
        }
    }
    return NULL;
}

static RoutineInstance* allocate_routine_instance(void) {
    if (routine_manager.active_count >= MAX_ACTIVE_ROUTINES) {
        return NULL;
    }
    return &routine_manager.active_routines[routine_manager.active_count++];
}

static void free_routine_instance(RoutineInstance* instance) {
    if (!instance) return;

    // Find instance index
    uint32_t index = instance - routine_manager.active_routines;
    if (index >= routine_manager.active_count) return;

    // Remove instance by shifting remaining ones
    if (index < (routine_manager.active_count - 1)) {
        memmove(&routine_manager.active_routines[index],
                &routine_manager.active_routines[index + 1],
                sizeof(RoutineInstance) * (routine_manager.active_count - index - 1));
    }

    routine_manager.active_count--;
}

bool Routine_Manager_Init(const RoutineManagerConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&routine_manager.critical);

    memcpy(&routine_manager.config, config, sizeof(RoutineManagerConfig));
    
    // Copy initial routines if provided
    if (config->routines && config->routine_count > 0) {
        uint32_t copy_count = (config->routine_count <= MAX_ROUTINES) ? 
                             config->routine_count : MAX_ROUTINES;
        memcpy(routine_manager.routines, config->routines, 
               sizeof(RoutineDefinition) * copy_count);
        routine_manager.routine_count = copy_count;
    } else {
        routine_manager.routine_count = 0;
    }

    routine_manager.active_count = 0;
    init_critical(&routine_manager.critical);
    routine_manager.initialized = true;

    exit_critical(&routine_manager.critical);
    return true;
}

void Routine_Manager_DeInit(void) {
    enter_critical(&routine_manager.critical);
    
    // Stop all active routines
    for (uint32_t i = 0; i < routine_manager.active_count; i++) {
        RoutineDefinition* routine = find_routine(routine_manager.active_routines[i].routine_id);
        if (routine && routine->stop_routine) {
            routine->stop_routine();
        }
    }
    
    memset(&routine_manager, 0, sizeof(RoutineManager));
    exit_critical(&routine_manager.critical);
}

bool Routine_Manager_StartRoutine(uint16_t routine_id, const uint8_t* data, uint16_t length) {
    if (!routine_manager.initialized) {
        return false;
    }

    enter_critical(&routine_manager.critical);

    RoutineDefinition* routine = find_routine(routine_id);
    if (!routine) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Check security level
    SessionState session_state = Session_Manager_GetState();
    if (session_state.security_level < routine->security_level) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Check if routine is already active
    if (find_active_routine(routine_id)) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Allocate new instance
    RoutineInstance* instance = allocate_routine_instance();
    if (!instance) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Initialize instance
    instance->routine_id = routine_id;
    instance->status = ROUTINE_STATUS_RUNNING;
    instance->last_result_code = 0;
    
    if (routine->timeout_ms > 0) {
        timer_start(&instance->timeout_timer, routine->timeout_ms);
    }

    // Start routine
    bool result = false;
    if (routine->start_routine) {
        result = routine->start_routine(data, length);
    }

    if (!result) {
        free_routine_instance(instance);
    } else if (routine_manager.config.status_callback) {
        routine_manager.config.status_callback(routine_id, ROUTINE_STATUS_RUNNING);
    }

    exit_critical(&routine_manager.critical);
    return result;
}

bool Routine_Manager_StopRoutine(uint16_t routine_id) {
    if (!routine_manager.initialized) {
        return false;
    }

    enter_critical(&routine_manager.critical);

    RoutineDefinition* routine = find_routine(routine_id);
    RoutineInstance* instance = find_active_routine(routine_id);

    if (!routine || !instance) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    bool result = true;
    if (routine->stop_routine) {
        result = routine->stop_routine();
    }

    if (result) {
        instance->status = ROUTINE_STATUS_STOPPED;
        if (routine_manager.config.status_callback) {
            routine_manager.config.status_callback(routine_id, ROUTINE_STATUS_STOPPED);
        }
        free_routine_instance(instance);
    }

    exit_critical(&routine_manager.critical);
    return result;
}

bool Routine_Manager_GetResult(uint16_t routine_id, RoutineResult* result) {
    if (!routine_manager.initialized || !result) {
        return false;
    }

    enter_critical(&routine_manager.critical);

    RoutineDefinition* routine = find_routine(routine_id);
    RoutineInstance* instance = find_active_routine(routine_id);

    if (!routine) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    bool success = false;

    if (instance) {
        // Routine is still active
        if (routine->get_result) {
            success = routine->get_result(result);
        }
        result->result_code = instance->last_result_code;
    } else {
        // Routine is not active, try to get final result
        if (routine->get_result) {
            success = routine->get_result(result);
        }
    }

    exit_critical(&routine_manager.critical);
    return success;
}

RoutineStatus Routine_Manager_GetStatus(uint16_t routine_id) {
    if (!routine_manager.initialized) {
        return ROUTINE_STATUS_INACTIVE;
    }

    enter_critical(&routine_manager.critical);

    RoutineInstance* instance = find_active_routine(routine_id);
    RoutineStatus status = instance ? instance->status : ROUTINE_STATUS_INACTIVE;

    exit_critical(&routine_manager.critical);
    return status;
}

bool Routine_Manager_AddRoutine(const RoutineDefinition* routine) {
    if (!routine_manager.initialized || !routine) {
        return false;
    }

    enter_critical(&routine_manager.critical);

    // Check if routine already exists
    if (find_routine(routine->routine_id)) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Check if we have space for new routine
    if (routine_manager.routine_count >= MAX_ROUTINES) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Add new routine
    memcpy(&routine_manager.routines[routine_manager.routine_count], 
           routine, sizeof(RoutineDefinition));
    routine_manager.routine_count++;

    exit_critical(&routine_manager.critical);
    return true;
}

bool Routine_Manager_RemoveRoutine(uint16_t routine_id) {
    if (!routine_manager.initialized) {
        return false;
    }

    enter_critical(&routine_manager.critical);

    // Find routine index
    int32_t index = -1;
    for (uint32_t i = 0; i < routine_manager.routine_count; i++) {
        if (routine_manager.routines[i].routine_id == routine_id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        exit_critical(&routine_manager.critical);
        return false;
    }

    // Stop routine if it's active
    RoutineInstance* instance = find_active_routine(routine_id);
    if (instance) {
        RoutineDefinition* routine = &routine_manager.routines[index];
        if (routine->stop_routine) {
            routine->stop_routine();
        }
        free_routine_instance(instance);
    }

    // Remove routine by shifting remaining ones
    if (index < (routine_manager.routine_count - 1)) {
        memmove(&routine_manager.routines[index],
                &routine_manager.routines[index + 1],
                sizeof(RoutineDefinition) * (routine_manager.routine_count - index - 1));
    }

    routine_manager.routine_count--;

    exit_critical(&routine_manager.critical);
    return true;
}

RoutineDefinition* Routine_Manager_GetRoutine(uint16_t routine_id) {
    if (!routine_manager.initialized) {
        return NULL;
    }

    enter_critical(&routine_manager.critical);
    RoutineDefinition* routine = find_routine(routine_id);
    exit_critical(&routine_manager.critical);

    return routine;
}

void Routine_Manager_ProcessTimeout(void) {
    if (!routine_manager.initialized) {
        return;
    }

    enter_critical(&routine_manager.critical);

    for (uint32_t i = 0; i < routine_manager.active_count; i++) {
        RoutineInstance* instance = &routine_manager.active_routines[i];
        RoutineDefinition* routine = find_routine(instance->routine_id);

        if (!routine) continue;

        if (routine->timeout_ms > 0 && timer_expired(&instance->timeout_timer)) {
            // Handle timeout
            if (routine->timeout_callback) {
                routine->timeout_callback();
            }

            instance->status = ROUTINE_STATUS_FAILED;
            instance->last_result_code = 0xFF; // Timeout error code

            if (routine_manager.config.status_callback) {
                routine_manager.config.status_callback(instance->routine_id, ROUTINE_STATUS_FAILED);
            }

            if (routine_manager.config.error_callback) {
                routine_manager.config.error_callback(instance->routine_id, 0xFF);
            }

            // Stop and remove the routine instance
            if (routine->stop_routine) {
                routine->stop_routine();
            }
            free_routine_instance(instance);
            i--; // Adjust index after removal
        }
    }

    exit_critical(&routine_manager.critical);
}

uint32_t Routine_Manager_GetActiveCount(void) {
    if (!routine_manager.initialized) {
        return 0;
    }
    return routine_manager.active_count;
}

void Routine_Manager_AbortAll(void) {
    if (!routine_manager.initialized) {
        return;
    }

    enter_critical(&routine_manager.critical);

    while (routine_manager.active_count > 0) {
        RoutineInstance* instance = &routine_manager.active_routines[0];
        RoutineDefinition* routine = find_routine(instance->routine_id);

        if (routine && routine->stop_routine) {
            routine->stop_routine();
        }

        instance->status = ROUTINE_STATUS_STOPPED;
        if (routine_manager.config.status_callback) {
            routine_manager.config.status_callback(instance->routine_id, ROUTINE_STATUS_STOPPED);
        }

        free_routine_instance(instance);
    }

    exit_critical(&routine_manager.critical);
}