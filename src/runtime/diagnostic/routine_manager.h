#ifndef CANT_ROUTINE_MANAGER_H
#define CANT_ROUTINE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Routine Status
typedef enum {
    ROUTINE_STATUS_INACTIVE,
    ROUTINE_STATUS_RUNNING,
    ROUTINE_STATUS_COMPLETED,
    ROUTINE_STATUS_FAILED,
    ROUTINE_STATUS_STOPPED
} RoutineStatus;

// Routine Control Type
typedef enum {
    ROUTINE_CONTROL_START = 0x01,
    ROUTINE_CONTROL_STOP = 0x02,
    ROUTINE_CONTROL_GET_RESULT = 0x03
} RoutineControlType;

// Routine Result
typedef struct {
    uint16_t result_code;
    uint8_t* data;
    uint16_t length;
} RoutineResult;

// Routine Definition
typedef struct {
    uint16_t routine_id;
    uint8_t security_level;
    uint32_t timeout_ms;
    bool (*start_routine)(const uint8_t* data, uint16_t length);
    bool (*stop_routine)(void);
    bool (*get_result)(RoutineResult* result);
    void (*timeout_callback)(void);
} RoutineDefinition;

// Routine Manager Configuration
typedef struct {
    RoutineDefinition* routines;
    uint32_t routine_count;
    void (*status_callback)(uint16_t routine_id, RoutineStatus status);
    void (*error_callback)(uint16_t routine_id, uint16_t error_code);
} RoutineManagerConfig;

// Routine Manager API
bool Routine_Manager_Init(const RoutineManagerConfig* config);
void Routine_Manager_DeInit(void);
bool Routine_Manager_StartRoutine(uint16_t routine_id, const uint8_t* data, uint16_t length);
bool Routine_Manager_StopRoutine(uint16_t routine_id);
bool Routine_Manager_GetResult(uint16_t routine_id, RoutineResult* result);
RoutineStatus Routine_Manager_GetStatus(uint16_t routine_id);
bool Routine_Manager_AddRoutine(const RoutineDefinition* routine);
bool Routine_Manager_RemoveRoutine(uint16_t routine_id);
RoutineDefinition* Routine_Manager_GetRoutine(uint16_t routine_id);
void Routine_Manager_ProcessTimeout(void);
uint32_t Routine_Manager_GetActiveCount(void);
void Routine_Manager_AbortAll(void);

#endif // CANT_ROUTINE_MANAGER_H 