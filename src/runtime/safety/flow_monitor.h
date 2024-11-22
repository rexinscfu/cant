#ifndef CANT_FLOW_MONITOR_H
#define CANT_FLOW_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

// Control Flow Checkpoint Types
typedef enum {
    FLOW_CHECKPOINT_START,
    FLOW_CHECKPOINT_END,
    FLOW_CHECKPOINT_BRANCH,
    FLOW_CHECKPOINT_LOOP,
    FLOW_CHECKPOINT_CALL,
    FLOW_CHECKPOINT_RETURN
} FlowCheckpointType;

// Flow Monitor Results
typedef enum {
    FLOW_MONITOR_OK,
    FLOW_MONITOR_ERROR_SEQUENCE,
    FLOW_MONITOR_ERROR_TIMING,
    FLOW_MONITOR_ERROR_RECURSION,
    FLOW_MONITOR_ERROR_STACK,
    FLOW_MONITOR_ERROR_CONFIG
} FlowMonitorResult;

// Checkpoint Configuration
typedef struct {
    uint32_t checkpoint_id;
    FlowCheckpointType type;
    uint32_t expected_next;
    uint32_t alternate_next;
    uint32_t max_time_ms;
    uint32_t min_time_ms;
} FlowCheckpointConfig;

// Flow Monitor Configuration
typedef struct {
    FlowCheckpointConfig* checkpoints;
    uint32_t checkpoint_count;
    uint32_t max_stack_depth;
    uint32_t max_recursion_depth;
    void (*error_callback)(FlowMonitorResult, uint32_t checkpoint_id);
} FlowMonitorConfig;

// Flow Monitor API
bool Flow_Monitor_Init(const FlowMonitorConfig* config);
void Flow_Monitor_DeInit(void);
FlowMonitorResult Flow_Monitor_Checkpoint(uint32_t checkpoint_id);
bool Flow_Monitor_IsSequenceValid(void);
uint32_t Flow_Monitor_GetErrorCount(void);
void Flow_Monitor_ResetErrorCount(void);
void Flow_Monitor_GetStatus(uint32_t* current_checkpoint, uint32_t* error_count);
bool Flow_Monitor_ValidateFlow(uint32_t start_checkpoint, uint32_t end_checkpoint);

#endif // CANT_FLOW_MONITOR_H 