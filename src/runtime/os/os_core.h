#ifndef CANT_OS_CORE_H
#define CANT_OS_CORE_H

#include "os_types.h"

// OS Control API
StatusType StartOS(AppModeType mode);
void ShutdownOS(StatusType error);
AppModeType GetActiveApplicationMode(void);

// Task Management
StatusType ActivateTask(TaskType task_id);
StatusType TerminateTask(void);
StatusType ChainTask(TaskType task_id);
StatusType Schedule(void);
StatusType GetTaskID(TaskType* task_id);
StatusType GetTaskState(TaskType task_id, TaskStateType* state);

// Interrupt Handling
void DisableAllInterrupts(void);
void EnableAllInterrupts(void);
void SuspendAllInterrupts(void);
void ResumeAllInterrupts(void);
void SuspendOSInterrupts(void);
void ResumeOSInterrupts(void);

// Resource Management
StatusType GetResource(ResourceType res_id);
StatusType ReleaseResource(ResourceType res_id);

// Event Control
StatusType SetEvent(TaskType task_id, EventMaskType mask);
StatusType ClearEvent(EventMaskType mask);
StatusType GetEvent(TaskType task_id, EventMaskType* mask);
StatusType WaitEvent(EventMaskType mask);

// Alarm Control
StatusType GetAlarmBase(AlarmType alarm_id, void* info);
StatusType GetAlarm(AlarmType alarm_id, uint32_t* ticks);
StatusType SetRelAlarm(AlarmType alarm_id, uint32_t increment, uint32_t cycle);
StatusType SetAbsAlarm(AlarmType alarm_id, uint32_t start, uint32_t cycle);
StatusType CancelAlarm(AlarmType alarm_id);

// Counter Control
StatusType IncrementCounter(CounterType counter_id);
StatusType GetCounterValue(CounterType counter_id, uint32_t* value);
StatusType GetElapsedValue(CounterType counter_id, uint32_t* value, uint32_t* elapsed);

#endif // CANT_OS_CORE_H 