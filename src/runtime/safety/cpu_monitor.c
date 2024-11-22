#include "cpu_monitor.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

// CPU Register Test Patterns
static const uint32_t REGISTER_PATTERNS[] = {
    0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
    0x33333333, 0xCCCCCCCC, 0x0F0F0F0F, 0xF0F0F0F0
};

// Internal state
static struct {
    CPUMonitorConfig config;
    struct {
        uint32_t total_errors;
        CPUTestResult last_results[6];
        Timer test_timer;
        bool initialized;
    } state;
    CriticalSection critical;
} cpu_monitor;

// Register test implementation
static CPUTestResult test_registers(void) {
    volatile uint32_t test_reg;
    
    for (uint32_t i = 0; i < sizeof(REGISTER_PATTERNS)/sizeof(uint32_t); i++) {
        test_reg = REGISTER_PATTERNS[i];
        if (test_reg != REGISTER_PATTERNS[i]) {
            return CPU_TEST_FAILED_REGISTER;
        }
        
        __asm volatile (
            "mov r0, %0\n"
            "mov r1, %0\n"
            "mov r2, %0\n"
            "mov r3, %0\n"
            "cmp r0, r1\n"
            "bne test_fail\n"
            "cmp r1, r2\n"
            "bne test_fail\n"
            "cmp r2, r3\n"
            "bne test_fail\n"
            "b test_pass\n"
            "test_fail:\n"
            "mov %0, #1\n"
            "test_pass:\n"
            : "=r" (test_reg)
            : "r" (REGISTER_PATTERNS[i])
            : "r0", "r1", "r2", "r3"
        );
        
        if (test_reg != REGISTER_PATTERNS[i]) {
            return CPU_TEST_FAILED_REGISTER;
        }
    }
    
    return CPU_TEST_OK;
}

// ALU test implementation
static CPUTestResult test_alu(void) {
    volatile uint32_t result;
    volatile uint32_t operand1 = 0x55555555;
    volatile uint32_t operand2 = 0xAAAAAAAA;
    
    // Addition test
    result = operand1 + operand2;
    if (result != 0xFFFFFFFF) return CPU_TEST_FAILED_ALU;
    
    // Subtraction test
    result = operand2 - operand1;
    if (result != 0x55555555) return CPU_TEST_FAILED_ALU;
    
    // Multiplication test
    result = (operand1 & 0xFFFF) * (operand2 & 0xFFFF);
    if (result != 0x38E38E39) return CPU_TEST_FAILED_ALU;
    
    // Division test
    result = operand2 / (operand1 & 0xFF);
    if (result != 0x1C71C71C) return CPU_TEST_FAILED_ALU;
    
    // Logical operations
    result = operand1 & operand2;
    if (result != 0x00000000) return CPU_TEST_FAILED_ALU;
    
    result = operand1 | operand2;
    if (result != 0xFFFFFFFF) return CPU_TEST_FAILED_ALU;
    
    result = operand1 ^ operand2;
    if (result != 0xFFFFFFFF) return CPU_TEST_FAILED_ALU;
    
    return CPU_TEST_OK;
}

// FPU test implementation
static CPUTestResult test_fpu(void) {
    volatile float test_float = 3.14159f;
    volatile float result;
    
    // Basic arithmetic
    result = test_float * 2.0f;
    if (result != 6.28318f) return CPU_TEST_FAILED_FPU;
    
    result = test_float / 2.0f;
    if (result != 1.570795f) return CPU_TEST_FAILED_FPU;
    
    result = test_float + 1.0f;
    if (result != 4.14159f) return CPU_TEST_FAILED_FPU;
    
    result = test_float - 1.0f;
    if (result != 2.14159f) return CPU_TEST_FAILED_FPU;
    
    // Transcendental functions
    result = sinf(test_float);
    if (result != 0.0f) return CPU_TEST_FAILED_FPU;
    
    result = cosf(test_float);
    if (result != -1.0f) return CPU_TEST_FAILED_FPU;
    
    return CPU_TEST_OK;
}

// MPU test implementation
static CPUTestResult test_mpu(void) {
    volatile uint32_t* test_addr;
    volatile uint32_t test_value;
    
    // Test read-only region
    test_addr = (uint32_t*)0x08000000;  // Flash memory region
    test_value = *test_addr;
    
    __asm volatile (
        "push {r0-r3}\n"
        "mrs r0, CONTROL\n"
        "orr r0, r0, #1\n"  // Switch to unprivileged mode
        "msr CONTROL, r0\n"
        "isb\n"
        "str %1, [%0]\n"    // Try to write to read-only memory
        "mov %1, #0\n"
        "b mpu_test_done\n"
        "mpu_fault_handler:\n"
        "mov %1, #1\n"
        "mpu_test_done:\n"
        "mrs r0, CONTROL\n"
        "bic r0, r0, #1\n"  // Switch back to privileged mode
        "msr CONTROL, r0\n"
        "isb\n"
        "pop {r0-r3}\n"
        : "=r" (test_addr), "=r" (test_value)
        : "r" (test_addr), "r" (test_value)
        : "r0"
    );
    
    if (test_value != 1) return CPU_TEST_FAILED_MPU;
    
    return CPU_TEST_OK;
}

// Cache test implementation
static CPUTestResult test_cache(void) {
    volatile uint32_t* test_addr = (uint32_t*)0x20000000;  // SRAM region
    volatile uint32_t test_pattern = 0x55AA55AA;
    
    // Write test pattern
    *test_addr = test_pattern;
    
    // Flush cache
    __asm volatile ("dsb sy");
    __asm volatile ("isb sy");
    
    // Invalidate cache
    __asm volatile ("mcr p15, 0, r0, c7, c6, 0");
    __asm volatile ("dsb sy");
    __asm volatile ("isb sy");
    
    // Read back and verify
    if (*test_addr != test_pattern) return CPU_TEST_FAILED_CACHE;
    
    return CPU_TEST_OK;
}

// Lockstep test implementation
static CPUTestResult test_lockstep(void) {
    volatile uint32_t core1_result = 0;
    volatile uint32_t core2_result = 0;
    
    // Execute same operation on both cores
    __asm volatile (
        "push {r0-r3}\n"
        "mov r0, #0x55\n"
        "mov r1, #0xAA\n"
        "add r2, r0, r1\n"
        "str r2, %0\n"
        "pop {r0-r3}\n"
        : "=m" (core1_result)
        :
        : "r0", "r1", "r2"
    );
    
    // Read result from redundant core
    core2_result = *((volatile uint32_t*)0xE000EDF0);  // Lockstep comparison register
    
    if (core1_result != core2_result) return CPU_TEST_FAILED_LOCKSTEP;
    
    return CPU_TEST_OK;
}

bool CPU_Monitor_Init(const CPUMonitorConfig* config) {
    if (!config) return false;
    
    enter_critical(&cpu_monitor.critical);
    
    memcpy(&cpu_monitor.config, config, sizeof(CPUMonitorConfig));
    cpu_monitor.state.total_errors = 0;
    memset(cpu_monitor.state.last_results, 0, sizeof(cpu_monitor.state.last_results));
    
    timer_start(&cpu_monitor.state.test_timer, config->test_interval_ms);
    cpu_monitor.state.initialized = true;
    
    exit_critical(&cpu_monitor.critical);
    return true;
}

void CPU_Monitor_DeInit(void) {
    enter_critical(&cpu_monitor.critical);
    cpu_monitor.state.initialized = false;
    exit_critical(&cpu_monitor.critical);
}

void CPU_Monitor_Process(void) {
    if (!cpu_monitor.state.initialized) return;
    
    enter_critical(&cpu_monitor.critical);
    
    if (timer_expired(&cpu_monitor.state.test_timer)) {
        CPUTestResult result;
        
        if (cpu_monitor.config.enable_register_test) {
            result = test_registers();
            if (result != CPU_TEST_OK) {
                cpu_monitor.state.total_errors++;
                cpu_monitor.state.last_results[CPU_TEST_REGISTER] = result;
                if (cpu_monitor.config.error_callback) {
                    cpu_monitor.config.error_callback(CPU_TEST_REGISTER, result);
                }
            }
        }
        
        if (cpu_monitor.config.enable_alu_test) {
            result = test_alu();
            if (result != CPU_TEST_OK) {
                cpu_monitor.state.total_errors++;
                cpu_monitor.state.last_results[CPU_TEST_ALU] = result;
                if (cpu_monitor.config.error_callback) {
                    cpu_monitor.config.error_callback(CPU_TEST_ALU, result);
                }
            }
        }
        
        if (cpu_monitor.config.enable_fpu_test) {
            result = test_fpu();
            if (result != CPU_TEST_OK) {
                cpu_monitor.state.total_errors++;
                cpu_monitor.state.last_results[CPU_TEST_FPU] = result;
                if (cpu_monitor.config.error_callback) {
                    cpu_monitor.config.error_callback(CPU_TEST_FPU, result);
                }
            }
        }
        
        if (cpu_monitor.config.enable_mpu_test) {
            result = test_mpu();
            if (result != CPU_TEST_OK) {
                cpu_monitor.state.total_errors++;
                cpu_monitor.state.last_results[CPU_TEST_MPU] = result;
                if (cpu_monitor.config.error_callback) {
                    cpu_monitor.config.error_callback(CPU_TEST_MPU, result);
                }
            }
        }
        
        if (cpu_monitor.config.enable_cache_test) {
            result = test_cache();
            if (result != CPU_TEST_OK) {
                cpu_monitor.state.total_errors++;
                cpu_monitor.state.last_results[CPU_TEST_CACHE] = result;
                if (cpu_monitor.config.error_callback) {
                    cpu_monitor.config.error_callback(CPU_TEST_CACHE, result);
                }
            }
        }
        
        if (cpu_monitor.config.enable_lockstep) {
            result = test_lockstep();
            if (result != CPU_TEST_OK) {
                cpu_monitor.state.total_errors++;
                cpu_monitor.state.last_results[CPU_TEST_LOCKSTEP] = result;
                if (cpu_monitor.config.error_callback) {
                    cpu_monitor.config.error_callback(CPU_TEST_LOCKSTEP, result);
                }
            }
        }
        
        timer_start(&cpu_monitor.state.test_timer, 
                   cpu_monitor.config.test_interval_ms);
    }
    
    exit_critical(&cpu_monitor.critical);
}

CPUTestResult CPU_Monitor_RunTest(CPUTestType test) {
    if (!cpu_monitor.state.initialized) return CPU_TEST_FAILED_TIMEOUT;
    
    CPUTestResult result;
    
    enter_critical(&cpu_monitor.critical);
    
    switch (test) {
        case CPU_TEST_REGISTER:
            result = test_registers();
            break;
        case CPU_TEST_ALU:
            result = test_alu();
            break;
        case CPU_TEST_FPU:
            result = test_fpu();
            break;
        case CPU_TEST_MPU:
            result = test_mpu();
            break;
        case CPU_TEST_CACHE:
            result = test_cache();
            break;
        case CPU_TEST_LOCKSTEP:
            result = test_lockstep();
            break;
        default:
            result = CPU_TEST_FAILED_TIMEOUT;
            break;
    }
    
    if (result != CPU_TEST_OK) {
        cpu_monitor.state.total_errors++;
        cpu_monitor.state.last_results[test] = result;
        if (cpu_monitor.config.error_callback) {
            cpu_monitor.config.error_callback(test, result);
        }
    }
    
    exit_critical(&cpu_monitor.critical);
    return result;
}

bool CPU_Monitor_IsHealthy(void) {
    if (!cpu_monitor.state.initialized) return false;
    
    bool healthy = true;
    
    enter_critical(&cpu_monitor.critical);
    for (int i = 0; i < 6; i++) {
        if (cpu_monitor.state.last_results[i] != CPU_TEST_OK) {
            healthy = false;
            break;
        }
    }
    exit_critical(&cpu_monitor.critical);
    
    return healthy;
}

uint32_t CPU_Monitor_GetErrorCount(void) {
    return cpu_monitor.state.total_errors;
}

void CPU_Monitor_ResetErrorCount(void) {
    enter_critical(&cpu_monitor.critical);
    cpu_monitor.state.total_errors = 0;
    memset(cpu_monitor.state.last_results, 0, sizeof(cpu_monitor.state.last_results));
    exit_critical(&cpu_monitor.critical);
}

void CPU_Monitor_GetStatus(CPUTestResult* results, uint32_t* count) {
    if (!results || !count) return;
    
    enter_critical(&cpu_monitor.critical);
    memcpy(results, cpu_monitor.state.last_results, sizeof(cpu_monitor.state.last_results));
    *count = sizeof(cpu_monitor.state.last_results) / sizeof(CPUTestResult);
    exit_critical(&cpu_monitor.critical);
} 