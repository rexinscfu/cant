#include "security_manager.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

#define MAX_SECURITY_LEVELS 16
#define SEED_LENGTH 4
#define DEFAULT_SEED_MASK 0xA5A5A5A5

typedef struct {
    uint8_t level;
    uint8_t attempt_count;
    bool seed_requested;
    bool level_locked;
    Timer delay_timer;
    uint32_t last_seed;
} SecurityLevelState;

typedef struct {
    SecurityManagerConfig config;
    SecurityLevel levels[MAX_SECURITY_LEVELS];
    SecurityLevelState level_states[MAX_SECURITY_LEVELS];
    uint32_t level_count;
    uint8_t current_level;
    bool initialized;
    CriticalSection critical;
} SecurityManager;

static SecurityManager security_manager;

static SecurityLevel* find_security_level(uint8_t level) {
    for (uint32_t i = 0; i < security_manager.level_count; i++) {
        if (security_manager.levels[i].level == level) {
            return &security_manager.levels[i];
        }
    }
    return NULL;
}

static SecurityLevelState* find_level_state(uint8_t level) {
    for (uint32_t i = 0; i < security_manager.level_count; i++) {
        if (security_manager.levels[i].level == level) {
            return &security_manager.level_states[i];
        }
    }
    return NULL;
}

static uint32_t generate_default_seed(void) {
    Timer timer;
    timer_init();
    return (timer.start_time ^ DEFAULT_SEED_MASK);
}

static bool validate_default_key(uint32_t seed, uint32_t key) {
    // Example key validation algorithm (should be more complex in production)
    return (key == ((seed ^ 0x55AA55AA) + 0x12345678));
}

bool Security_Manager_Init(const SecurityManagerConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&security_manager.critical);

    memcpy(&security_manager.config, config, sizeof(SecurityManagerConfig));
    
    // Copy initial security levels if provided
    if (config->levels && config->level_count > 0) {
        uint32_t copy_count = (config->level_count <= MAX_SECURITY_LEVELS) ? 
                             config->level_count : MAX_SECURITY_LEVELS;
        memcpy(security_manager.levels, config->levels, 
               sizeof(SecurityLevel) * copy_count);
        security_manager.level_count = copy_count;

        // Initialize level states
        for (uint32_t i = 0; i < copy_count; i++) {
            security_manager.level_states[i].level = config->levels[i].level;
            security_manager.level_states[i].attempt_count = 0;
            security_manager.level_states[i].seed_requested = false;
            security_manager.level_states[i].level_locked = true;
            timer_init();
        }
    } else {
        security_manager.level_count = 0;
    }

    security_manager.current_level = 0;
    init_critical(&security_manager.critical);
    security_manager.initialized = true;

    exit_critical(&security_manager.critical);
    return true;
}

void Security_Manager_DeInit(void) {
    enter_critical(&security_manager.critical);
    memset(&security_manager, 0, sizeof(SecurityManager));
    exit_critical(&security_manager.critical);
}

bool Security_Manager_RequestSeed(uint8_t level, uint8_t* seed, uint16_t* length) {
    if (!security_manager.initialized || !seed || !length || *length < SEED_LENGTH) {
        return false;
    }

    enter_critical(&security_manager.critical);

    SecurityLevel* sec_level = find_security_level(level);
    SecurityLevelState* level_state = find_level_state(level);

    if (!sec_level || !level_state) {
        exit_critical(&security_manager.critical);
        return false;
    }

    // Check if delay timer is still running
    if (timer_expired(&level_state->delay_timer)) {
        exit_critical(&security_manager.critical);
        if (security_manager.config.violation_callback) {
            security_manager.config.violation_callback(level, SECURITY_VIOLATION_DELAY_NOT_EXPIRED);
        }
        return false;
    }

    // Generate seed
    uint32_t new_seed;
    if (sec_level->seed_generator) {
        if (!sec_level->seed_generator(level, seed, length)) {
            exit_critical(&security_manager.critical);
            return false;
        }
        memcpy(&new_seed, seed, sizeof(uint32_t));
    } else {
        new_seed = generate_default_seed();
        memcpy(seed, &new_seed, SEED_LENGTH);
        *length = SEED_LENGTH;
    }

    level_state->last_seed = new_seed;
    level_state->seed_requested = true;

    exit_critical(&security_manager.critical);
    return true;
}

bool Security_Manager_ValidateKey(uint8_t level, const uint8_t* key, uint16_t length) {
    if (!security_manager.initialized || !key || length < SEED_LENGTH) {
        return false;
    }

    enter_critical(&security_manager.critical);

    SecurityLevel* sec_level = find_security_level(level);
    SecurityLevelState* level_state = find_level_state(level);

    if (!sec_level || !level_state) {
        exit_critical(&security_manager.critical);
        return false;
    }

    // Check if seed was requested
    if (!level_state->seed_requested) {
        exit_critical(&security_manager.critical);
        if (security_manager.config.violation_callback) {
            security_manager.config.violation_callback(level, SECURITY_VIOLATION_SEQUENCE_ERROR);
        }
        return false;
    }

    // Check attempt limit
    if (level_state->attempt_count >= sec_level->max_attempts) {
        // Start delay timer
        timer_start(&level_state->delay_timer, sec_level->delay_time_ms);
        level_state->attempt_count = 0;
        level_state->seed_requested = false;
        
        exit_critical(&security_manager.critical);
        if (security_manager.config.violation_callback) {
            security_manager.config.violation_callback(level, SECURITY_VIOLATION_ATTEMPT_LIMIT);
        }
        return false;
    }

    bool validation_result;
    if (sec_level->key_validator) {
        validation_result = sec_level->key_validator(level, key, length);
    } else {
        uint32_t received_key;
        memcpy(&received_key, key, sizeof(uint32_t));
        validation_result = validate_default_key(level_state->last_seed, received_key);
    }

    if (validation_result) {
        level_state->level_locked = false;
        level_state->attempt_count = 0;
        security_manager.current_level = level;
        
        if (security_manager.config.security_callback) {
            security_manager.config.security_callback(level, true);
        }
    } else {
        level_state->attempt_count++;
        if (security_manager.config.violation_callback) {
            security_manager.config.violation_callback(level, SECURITY_VIOLATION_INVALID_KEY);
        }
    }

    level_state->seed_requested = false;
    exit_critical(&security_manager.critical);
    return validation_result;
}

bool Security_Manager_IsLevelUnlocked(uint8_t level) {
    if (!security_manager.initialized) {
        return false;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelState* level_state = find_level_state(level);
    bool is_unlocked = level_state && !level_state->level_locked;
    
    exit_critical(&security_manager.critical);
    return is_unlocked;
}

bool Security_Manager_LockLevel(uint8_t level) {
    if (!security_manager.initialized) {
        return false;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelState* level_state = find_level_state(level);
    if (!level_state) {
        exit_critical(&security_manager.critical);
        return false;
    }

    level_state->level_locked = true;
    level_state->attempt_count = 0;
    level_state->seed_requested = false;

    if (security_manager.current_level == level) {
        security_manager.current_level = 0;
    }

    if (security_manager.config.security_callback) {
        security_manager.config.security_callback(level, false);
    }

    exit_critical(&security_manager.critical);
    return true;
}

bool Security_Manager_UnlockLevel(uint8_t level) {
    if (!security_manager.initialized) {
        return false;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevel* sec_level = find_security_level(level);
    SecurityLevelState* level_state = find_level_state(level);
    
    if (!sec_level || !level_state) {
        exit_critical(&security_manager.critical);
        return false;
    }

    level_state->level_locked = false;
    level_state->attempt_count = 0;
    level_state->seed_requested = false;
    security_manager.current_level = level;

    if (security_manager.config.security_callback) {
        security_manager.config.security_callback(level, true);
    }

    exit_critical(&security_manager.critical);
    return true;
}

uint8_t Security_Manager_GetCurrentLevel(void) {
    if (!security_manager.initialized) {
        return 0;
    }
    return security_manager.current_level;
}

uint32_t Security_Manager_GetRemainingDelay(uint8_t level) {
    if (!security_manager.initialized) {
        return 0;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelState* level_state = find_level_state(level);
    uint32_t remaining = 0;
    
    if (level_state) {
        remaining = timer_remaining(&level_state->delay_timer);
    }
    
    exit_critical(&security_manager.critical);
    return remaining;
}

uint8_t Security_Manager_GetRemainingAttempts(uint8_t level) {
    if (!security_manager.initialized) {
        return 0;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevel* sec_level = find_security_level(level);
    SecurityLevelState* level_state = find_level_state(level);
    
    uint8_t remaining = 0;
    if (sec_level && level_state) {
        remaining = sec_level->max_attempts - level_state->attempt_count;
    }
    
    exit_critical(&security_manager.critical);
    return remaining;
}

void Security_Manager_ResetAttempts(uint8_t level) {
    if (!security_manager.initialized) {
        return;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelState* level_state = find_level_state(level);
    if (level_state) {
        level_state->attempt_count = 0;
        level_state->seed_requested = false;
    }
    
    exit_critical(&security_manager.critical);
}

bool Security_Manager_AddSecurityLevel(const SecurityLevel* level) {
    if (!security_manager.initialized || !level) {
        return false;
    }

    enter_critical(&security_manager.critical);

    // Check if level already exists
    if (find_security_level(level->level)) {
        exit_critical(&security_manager.critical);
        return false;
    }

    // Check if we have space for new level
    if (security_manager.level_count >= MAX_SECURITY_LEVELS) {
        exit_critical(&security_manager.critical);
        return false;
    }

    // Add new level
    memcpy(&security_manager.levels[security_manager.level_count], 
           level, sizeof(SecurityLevel));
    
    // Initialize level state
    security_manager.level_states[security_manager.level_count].level = level->level;
    security_manager.level_states[security_manager.level_count].attempt_count = 0;
    security_manager.level_states[security_manager.level_count].seed_requested = false;
    security_manager.level_states[security_manager.level_count].level_locked = true;
    timer_init();

    security_manager.level_count++;

    exit_critical(&security_manager.critical);
    return true;
}

bool Security_Manager_RemoveSecurityLevel(uint8_t level) {
    if (!security_manager.initialized) {
        return false;
    }

    enter_critical(&security_manager.critical);

    // Find level index
    int32_t index = -1;
    for (uint32_t i = 0; i < security_manager.level_count; i++) {
        if (security_manager.levels[i].level == level) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        exit_critical(&security_manager.critical);
        return false;
    }

    // Remove level by shifting remaining ones
    if (index < (security_manager.level_count - 1)) {
        memmove(&security_manager.levels[index],
                &security_manager.levels[index + 1],
                sizeof(SecurityLevel) * (security_manager.level_count - index - 1));
        memmove(&security_manager.level_states[index],
                &security_manager.level_states[index + 1],
                sizeof(SecurityLevelState) * (security_manager.level_count - index - 1));
    }

    security_manager.level_count--;

    if (security_manager.current_level == level) {
        security_manager.current_level = 0;
    }

    exit_critical(&security_manager.critical);
    return true;
}

SecurityLevel* Security_Manager_GetSecurityLevel(uint8_t level) {
    if (!security_manager.initialized) {
        return NULL;
    }

    enter_critical(&security_manager.critical);
    SecurityLevel* sec_level = find_security_level(level);
    exit_critical(&security_manager.critical);

    return sec_level;
}

void Security_Manager_ProcessTimeout(void) {
    if (!security_manager.initialized) {
        return;
    }

    enter_critical(&security_manager.critical);

    for (uint32_t i = 0; i < security_manager.level_count; i++) {
        SecurityLevelState* level_state = &security_manager.level_states[i];
        
        // Reset attempt count if delay timer expired
        if (timer_expired(&level_state->delay_timer)) {
            level_state->attempt_count = 0;
            level_state->seed_requested = false;
        }
    }

    exit_critical(&security_manager.critical);
}