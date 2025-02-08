#include "security_manager.h"
#include "security_state.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

#define MAX_SECURITY_LEVELS 16
#define SEED_LENGTH 4
#define DEFAULT_SEED_MASK 0xA5A5A5A5

typedef struct {
    SecurityLevel level_config;
    SecurityStateContext state;
} SecurityLevelEntry;

typedef struct {
    SecurityManagerConfig config;
    SecurityLevelEntry levels[MAX_SECURITY_LEVELS];
    uint32_t level_count;
    uint8_t current_level;
    bool initialized;
    CriticalSection critical;
} SecurityManager;

static SecurityManager security_manager;

static SecurityLevelEntry* find_security_level(uint8_t level) {
    for (uint32_t i = 0; i < security_manager.level_count; i++) {
        if (security_manager.levels[i].level_config.level == level) {
            return &security_manager.levels[i];
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
    if (!config) return false;

    enter_critical(&security_manager.critical);

    memcpy(&security_manager.config, config, sizeof(SecurityManagerConfig));
    
    if (config->levels && config->level_count > 0) {
        uint32_t copy_count = (config->level_count <= MAX_SECURITY_LEVELS) ? 
                             config->level_count : MAX_SECURITY_LEVELS;
                             
        for (uint32_t i = 0; i < copy_count; i++) {
            memcpy(&security_manager.levels[i].level_config, &config->levels[i], 
                   sizeof(SecurityLevel));
            Security_State_Init(&security_manager.levels[i].state, 
                              config->levels[i].level);
        }
        security_manager.level_count = copy_count;
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

    SecurityLevelEntry* entry = find_security_level(level);
    if (!entry) {
        exit_critical(&security_manager.critical);
        return false;
    }

    uint32_t seed_value;
    bool result = Security_State_RequestSeed(&entry->state, &seed_value);
    
    if (result) {
        memcpy(seed, &seed_value, SEED_LENGTH);
        *length = SEED_LENGTH;
    } else if (Security_State_GetState(&entry->state) == SECURITY_STATE_DELAY_ACTIVE) {
        if (security_manager.config.violation_callback) {
            security_manager.config.violation_callback(level, 
                SECURITY_VIOLATION_DELAY_NOT_EXPIRED);
        }
    }

    exit_critical(&security_manager.critical);
    return result;
}

bool Security_Manager_ValidateKey(uint8_t level, const uint8_t* key, uint16_t length) {
    if (!security_manager.initialized || !key || length < SEED_LENGTH) {
        return false;
    }

    enter_critical(&security_manager.critical);

    SecurityLevelEntry* entry = find_security_level(level);
    if (!entry) {
        exit_critical(&security_manager.critical);
        return false;
    }

    uint32_t key_value;
    memcpy(&key_value, key, sizeof(uint32_t));

    bool result = Security_State_ValidateKey(&entry->state, key_value);
    
    if (result) {
        security_manager.current_level = level;
        if (security_manager.config.security_callback) {
            security_manager.config.security_callback(level, true);
        }
    } else {
        SecurityState state = Security_State_GetState(&entry->state);
        if (state == SECURITY_STATE_DELAY_ACTIVE) {
            if (security_manager.config.violation_callback) {
                security_manager.config.violation_callback(level, 
                    SECURITY_VIOLATION_ATTEMPT_LIMIT);
            }
        } else {
            if (security_manager.config.violation_callback) {
                security_manager.config.violation_callback(level, 
                    SECURITY_VIOLATION_INVALID_KEY);
            }
        }
    }

    exit_critical(&security_manager.critical);
    return result;
}

bool Security_Manager_IsLevelUnlocked(uint8_t level) {
    if (!security_manager.initialized) return false;

    enter_critical(&security_manager.critical);
    
    SecurityLevelEntry* entry = find_security_level(level);
    bool is_unlocked = entry && 
        (Security_State_GetState(&entry->state) == SECURITY_STATE_UNLOCKED);
    
    exit_critical(&security_manager.critical);
    return is_unlocked;
}

bool Security_Manager_LockLevel(uint8_t level) {
    if (!security_manager.initialized) return false;

    enter_critical(&security_manager.critical);
    
    SecurityLevelEntry* entry = find_security_level(level);
    if (!entry) {
        exit_critical(&security_manager.critical);
        return false;
    }

    bool result = Security_State_Lock(&entry->state);
    
    if (result && security_manager.current_level == level) {
        security_manager.current_level = 0;
    }

    exit_critical(&security_manager.critical);
    return result;
}

uint8_t Security_Manager_GetCurrentLevel(void) {
    return security_manager.initialized ? security_manager.current_level : 0;
}

uint32_t Security_Manager_GetRemainingDelay(uint8_t level) {
    if (!security_manager.initialized) {
        return 0;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelEntry* entry = find_security_level(level);
    uint32_t remaining = 0;
    
    if (entry) {
        remaining = timer_remaining(&entry->state.delay_timer);
    }
    
    exit_critical(&security_manager.critical);
    return remaining;
}

uint8_t Security_Manager_GetRemainingAttempts(uint8_t level) {
    if (!security_manager.initialized) {
        return 0;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelEntry* entry = find_security_level(level);
    
    uint8_t remaining = 0;
    if (entry) {
        remaining = entry->level_config.max_attempts - entry->state.attempt_count;
    }
    
    exit_critical(&security_manager.critical);
    return remaining;
}

void Security_Manager_ResetAttempts(uint8_t level) {
    if (!security_manager.initialized) {
        return;
    }

    enter_critical(&security_manager.critical);
    
    SecurityLevelEntry* entry = find_security_level(level);
    if (entry) {
        entry->state.attempt_count = 0;
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
    memcpy(&security_manager.levels[security_manager.level_count].level_config, 
           level, sizeof(SecurityLevel));
    
    // Initialize level state
    Security_State_Init(&security_manager.levels[security_manager.level_count].state, 
                      level->level);

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
        if (security_manager.levels[i].level_config.level == level) {
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
                sizeof(SecurityLevelEntry) * (security_manager.level_count - index - 1));
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
    SecurityLevelEntry* entry = find_security_level(level);
    exit_critical(&security_manager.critical);

    return &entry->level_config;
}

void Security_Manager_ProcessTimeout(void) {
    if (!security_manager.initialized) return;

    enter_critical(&security_manager.critical);
    
    for (uint32_t i = 0; i < security_manager.level_count; i++) {
        Security_State_ProcessTimeout(&security_manager.levels[i].state);
    }
    
    exit_critical(&security_manager.critical);
}