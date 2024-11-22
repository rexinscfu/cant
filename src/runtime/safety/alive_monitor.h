#ifndef CANT_ALIVE_MONITOR_H
#define CANT_ALIVE_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

// Alive Monitor States
typedef enum {
    ALIVE_STATE_HEALTHY,
    ALIVE_STATE_DEGRADED,
    ALIVE_STATE_CRITICAL,
    ALIVE_STATE_FAILED
} AliveState;

// Task Monitoring Config
typedef struct {
    uint32_t task_id;
    uint32_t deadline_ms;
    uint32_t min_cycle_ms;
    uint32_t max_cycle_ms;
    uint32_t max_jitter_ms;
    uint32_t tolerance_count;
    bool is_critical;
} AliveTaskConfig;

// Alive Monitor Config
typedef struct {
    AliveTaskConfig* tasks;
    uint32_t task_count;
    uint32_t supervision_interval_ms;
    void (*state_change_callback)(uint32_t task_id, AliveState new_state);
    void (*error_callback)(uint32_t task_id, uint32_t error_code);
} AliveMonitorConfig;

// Alive Monitor API
bool Alive_Monitor_Init(const AliveMonitorConfig* config);
void Alive_Monitor_DeInit(void);
void Alive_Monitor_Process(void);
void Alive_Monitor_ReportAlive(uint32_t task_id);
AliveState Alive_Monitor_GetTaskState(uint32_t task_id);
AliveState Alive_Monitor_GetSystemState(void);
uint32_t Alive_Monitor_GetDeadlineMisses(uint32_t task_id);
void Alive_Monitor_ResetStatistics(void);
bool Alive_Monitor_IsTaskHealthy(uint32_t task_id);

#endif // CANT_ALIVE_MONITOR_H 