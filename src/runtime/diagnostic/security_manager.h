#ifndef CANT_SECURITY_MANAGER_H
#define CANT_SECURITY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Security Level Definition
typedef struct {
    uint8_t level;
    uint32_t delay_time_ms;
    uint8_t max_attempts;
    bool (*seed_generator)(uint8_t level, uint8_t* seed, uint16_t* length);
    bool (*key_validator)(uint8_t level, const uint8_t* key, uint16_t length);
} SecurityLevel;

// Security Manager Configuration
typedef struct {
    SecurityLevel* levels;
    uint32_t level_count;
    uint32_t default_delay_time_ms;
    uint8_t default_max_attempts;
    void (*security_callback)(uint8_t level, bool access_granted);
    void (*violation_callback)(uint8_t level, uint32_t violation_type);
} SecurityManagerConfig;

// Security Violation Types
typedef enum {
    SECURITY_VIOLATION_INVALID_KEY = 0x01,
    SECURITY_VIOLATION_SEQUENCE_ERROR = 0x02,
    SECURITY_VIOLATION_ATTEMPT_LIMIT = 0x03,
    SECURITY_VIOLATION_DELAY_NOT_EXPIRED = 0x04,
    SECURITY_VIOLATION_INVALID_LEVEL = 0x05
} SecurityViolationType;

// Security Manager API
bool Security_Manager_Init(const SecurityManagerConfig* config);
void Security_Manager_DeInit(void);
bool Security_Manager_RequestSeed(uint8_t level, uint8_t* seed, uint16_t* length);
bool Security_Manager_ValidateKey(uint8_t level, const uint8_t* key, uint16_t length);
bool Security_Manager_IsLevelUnlocked(uint8_t level);
bool Security_Manager_LockLevel(uint8_t level);
bool Security_Manager_UnlockLevel(uint8_t level);
uint8_t Security_Manager_GetCurrentLevel(void);
uint32_t Security_Manager_GetRemainingDelay(uint8_t level);
uint8_t Security_Manager_GetRemainingAttempts(uint8_t level);
void Security_Manager_ResetAttempts(uint8_t level);
bool Security_Manager_AddSecurityLevel(const SecurityLevel* level);
bool Security_Manager_RemoveSecurityLevel(uint8_t level);
SecurityLevel* Security_Manager_GetSecurityLevel(uint8_t level);
void Security_Manager_ProcessTimeout(void);

#endif // CANT_SECURITY_MANAGER_H 