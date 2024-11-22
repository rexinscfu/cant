#ifndef CANT_CPU_MONITOR_H
#define CANT_CPU_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

// CPU Test Types
typedef enum {
    CPU_TEST_REGISTER,
    CPU_TEST_ALU,
    CPU_TEST_FPU,
    CPU_TEST_MPU,
    CPU_TEST_CACHE,
    CPU_TEST_LOCKSTEP
} CPUTestType;

// CPU Test Results
typedef enum {
    CPU_TEST_OK,
    CPU_TEST_FAILED_REGISTER,
    CPU_TEST_FAILED_ALU,
    CPU_TEST_FAILED_FPU,
    CPU_TEST_FAILED_MPU,
    CPU_TEST_FAILED_CACHE,
    CPU_TEST_FAILED_LOCKSTEP,
    CPU_TEST_FAILED_TIMEOUT
} CPUTestResult;

// CPU Monitor Configuration
typedef struct {
    uint32_t test_interval_ms;
    bool enable_register_test;
    bool enable_alu_test;
    bool enable_fpu_test;
    bool enable_mpu_test;
    bool enable_cache_test;
    bool enable_lockstep;
    uint32_t timeout_ms;
    void (*error_callback)(CPUTestType, CPUTestResult);
} CPUMonitorConfig;

// CPU Monitor API
bool CPU_Monitor_Init(const CPUMonitorConfig* config);
void CPU_Monitor_DeInit(void);
void CPU_Monitor_Process(void);
CPUTestResult CPU_Monitor_RunTest(CPUTestType test);
bool CPU_Monitor_IsHealthy(void);
uint32_t CPU_Monitor_GetErrorCount(void);
void CPU_Monitor_ResetErrorCount(void);
void CPU_Monitor_GetStatus(CPUTestResult* results, uint32_t* count);

#endif // CANT_CPU_MONITOR_H 