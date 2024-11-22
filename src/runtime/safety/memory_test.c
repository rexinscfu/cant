#include "memory_test.h"
#include <string.h>
#include "../utils/timer.h"
#include "../utils/crc.h"
#include "../os/critical.h"

// Default test patterns if none provided
static const uint32_t DEFAULT_PATTERNS[] = {
    0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
    0x33333333, 0xCCCCCCCC, 0x0F0F0F0F, 0xF0F0F0F0
};

// Internal state
static struct {
    MemoryTestConfig config;
    struct {
        uint32_t total_errors;
        MemoryTestResult* last_results;
        Timer test_timer;
        uint32_t current_region;
        uint32_t current_pattern;
        bool initialized;
    } state;
    CriticalSection critical;
} memory_test;

// March C test implementation
static MemoryTestResult march_c_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    volatile uint32_t* start = (volatile uint32_t*)region->start_address;
    volatile uint32_t* end = start + (region->size / sizeof(uint32_t));
    volatile uint32_t* ptr;
    
    // Phase 1: Write 0
    for (ptr = start; ptr < end; ptr++) {
        *ptr = 0;
        if (*ptr != 0) return MEM_TEST_FAILED_WRITE;
    }
    
    // Phase 2: Read 0, write 1
    for (ptr = start; ptr < end; ptr++) {
        if (*ptr != 0) return MEM_TEST_FAILED_READ;
        *ptr = 0xFFFFFFFF;
        if (*ptr != 0xFFFFFFFF) return MEM_TEST_FAILED_WRITE;
    }
    
    // Phase 3: Read 1, write 0
    for (ptr = start; ptr < end; ptr++) {
        if (*ptr != 0xFFFFFFFF) return MEM_TEST_FAILED_READ;
        *ptr = 0;
        if (*ptr != 0) return MEM_TEST_FAILED_WRITE;
    }
    
    // Phase 4: Read 0
    for (ptr = start; ptr < end; ptr++) {
        if (*ptr != 0) return MEM_TEST_FAILED_READ;
    }
    
    return MEM_TEST_OK;
}

// Checkerboard pattern test
static MemoryTestResult checkerboard_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    volatile uint32_t* start = (volatile uint32_t*)region->start_address;
    volatile uint32_t* end = start + (region->size / sizeof(uint32_t));
    volatile uint32_t* ptr;
    
    // Write checkerboard pattern
    for (ptr = start; ptr < end; ptr += 2) {
        *ptr = 0x55555555;
        *(ptr + 1) = 0xAAAAAAAA;
    }
    
    // Verify pattern
    for (ptr = start; ptr < end; ptr += 2) {
        if (*ptr != 0x55555555) return MEM_TEST_FAILED_PATTERN;
        if (*(ptr + 1) != 0xAAAAAAAA) return MEM_TEST_FAILED_PATTERN;
    }
    
    // Invert pattern
    for (ptr = start; ptr < end; ptr += 2) {
        *ptr = 0xAAAAAAAA;
        *(ptr + 1) = 0x55555555;
    }
    
    // Verify inverted pattern
    for (ptr = start; ptr < end; ptr += 2) {
        if (*ptr != 0xAAAAAAAA) return MEM_TEST_FAILED_PATTERN;
        if (*(ptr + 1) != 0x55555555) return MEM_TEST_FAILED_PATTERN;
    }
    
    return MEM_TEST_OK;
}

// Walking 1's test
static MemoryTestResult walking_1_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    volatile uint32_t* start = (volatile uint32_t*)region->start_address;
    volatile uint32_t* end = start + (region->size / sizeof(uint32_t));
    volatile uint32_t* ptr;
    uint32_t pattern = 1;
    
    for (uint32_t bit = 0; bit < 32; bit++) {
        // Write pattern
        for (ptr = start; ptr < end; ptr++) {
            *ptr = pattern;
            if (*ptr != pattern) return MEM_TEST_FAILED_WRITE;
        }
        
        // Verify pattern
        for (ptr = start; ptr < end; ptr++) {
            if (*ptr != pattern) return MEM_TEST_FAILED_READ;
        }
        
        pattern <<= 1;
    }
    
    return MEM_TEST_OK;
}

// Walking 0's test
static MemoryTestResult walking_0_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    volatile uint32_t* start = (volatile uint32_t*)region->start_address;
    volatile uint32_t* end = start + (region->size / sizeof(uint32_t));
    volatile uint32_t* ptr;
    uint32_t pattern = 0xFFFFFFFE;
    
    for (uint32_t bit = 0; bit < 32; bit++) {
        // Write pattern
        for (ptr = start; ptr < end; ptr++) {
            *ptr = pattern;
            if (*ptr != pattern) return MEM_TEST_FAILED_WRITE;
        }
        
        // Verify pattern
        for (ptr = start; ptr < end; ptr++) {
            if (*ptr != pattern) return MEM_TEST_FAILED_READ;
        }
        
        pattern = ~(1 << bit);
    }
    
    return MEM_TEST_OK;
}

// Address fault test
static MemoryTestResult address_fault_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    volatile uint32_t* start = (volatile uint32_t*)region->start_address;
    volatile uint32_t* end = start + (region->size / sizeof(uint32_t));
    volatile uint32_t* ptr;
    
    // Write address as data
    for (ptr = start; ptr < end; ptr++) {
        *ptr = (uint32_t)ptr;
    }
    
    // Verify address pattern
    for (ptr = start; ptr < end; ptr++) {
        if (*ptr != (uint32_t)ptr) return MEM_TEST_FAILED_ADDRESS;
    }
    
    return MEM_TEST_OK;
}

// Flash CRC test
static MemoryTestResult flash_crc_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    if (region->type != MEM_REGION_FLASH && region->type != MEM_REGION_ROM) {
        return MEM_TEST_FAILED_CRC;
    }
    
    const uint8_t* start = (const uint8_t*)region->start_address;
    uint32_t crc = calculate_crc32(start, region->size);
    
    // Compare with stored CRC at end of region
    uint32_t stored_crc = *(uint32_t*)(region->start_address + region->size - 4);
    
    return (crc == stored_crc) ? MEM_TEST_OK : MEM_TEST_FAILED_CRC;
}

// RAM pattern test
static MemoryTestResult ram_pattern_test(uint32_t region_index) {
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    volatile uint32_t* start = (volatile uint32_t*)region->start_address;
    volatile uint32_t* end = start + (region->size / sizeof(uint32_t));
    volatile uint32_t* ptr;
    
    const uint32_t* patterns = memory_test.config.test_patterns ? 
                              memory_test.config.test_patterns : DEFAULT_PATTERNS;
    uint32_t pattern_count = memory_test.config.pattern_count ? 
                            memory_test.config.pattern_count : 
                            sizeof(DEFAULT_PATTERNS)/sizeof(uint32_t);
    
    for (uint32_t i = 0; i < pattern_count; i++) {
        uint32_t pattern = patterns[i];
        
        // Write pattern
        for (ptr = start; ptr < end; ptr++) {
            *ptr = pattern;
        }
        
        // Verify pattern
        for (ptr = start; ptr < end; ptr++) {
            if (*ptr != pattern) return MEM_TEST_FAILED_PATTERN;
        }
    }
    
    return MEM_TEST_OK;
}

bool Memory_Test_Init(const MemoryTestConfig* config) {
    if (!config || !config->regions || config->region_count == 0) {
        return false;
    }
    
    enter_critical(&memory_test.critical);
    
    memcpy(&memory_test.config, config, sizeof(MemoryTestConfig));
    
    // Allocate result storage
    memory_test.state.last_results = calloc(config->region_count, 
                                          sizeof(MemoryTestResult));
    if (!memory_test.state.last_results) {
        exit_critical(&memory_test.critical);
        return false;
    }
    
    memory_test.state.total_errors = 0;
    memory_test.state.current_region = 0;
    memory_test.state.current_pattern = 0;
    
    timer_start(&memory_test.state.test_timer, config->test_interval_ms);
    memory_test.state.initialized = true;
    
    exit_critical(&memory_test.critical);
    return true;
}

void Memory_Test_DeInit(void) {
    enter_critical(&memory_test.critical);
    
    if (memory_test.state.last_results) {
        free(memory_test.state.last_results);
        memory_test.state.last_results = NULL;
    }
    
    memory_test.state.initialized = false;
    
    exit_critical(&memory_test.critical);
}

void Memory_Test_Process(void) {
    if (!memory_test.state.initialized) return;
    
    enter_critical(&memory_test.critical);
    
    if (timer_expired(&memory_test.state.test_timer)) {
        // Run tests for current region
        if (memory_test.config.regions[memory_test.state.current_region].run_background_test) {
            MemoryTestResult result = MEM_TEST_OK;
            
            switch (memory_test.state.current_pattern) {
                case 0:
                    result = march_c_test(memory_test.state.current_region);
                    break;
                case 1:
                    result = checkerboard_test(memory_test.state.current_region);
                    break;
                case 2:
                    result = walking_1_test(memory_test.state.current_region);
                    break;
                case 3:
                    result = walking_0_test(memory_test.state.current_region);
                    break;
                case 4:
                    result = address_fault_test(memory_test.state.current_region);
                    break;
                case 5:
                    if (memory_test.config.regions[memory_test.state.current_region].type == MEM_REGION_FLASH ||
                        memory_test.config.regions[memory_test.state.current_region].type == MEM_REGION_ROM) {
                        result = flash_crc_test(memory_test.state.current_region);
                    }
                    break;
                case 6:
                    if (memory_test.config.regions[memory_test.state.current_region].type == MEM_REGION_RAM) {
                        result = ram_pattern_test(memory_test.state.current_region);
                    }
                    break;
            }
            
            if (result != MEM_TEST_OK) {
                memory_test.state.total_errors++;
                memory_test.state.last_results[memory_test.state.current_region] = result;
                
                if (memory_test.config.error_callback) {
                    memory_test.config.error_callback(memory_test.state.current_pattern,
                                                    result,
                                                    memory_test.config.regions[memory_test.state.current_region].start_address);
                }
            }
        }
        
        // Move to next pattern/region
        memory_test.state.current_pattern++;
        if (memory_test.state.current_pattern > 6) {
            memory_test.state.current_pattern = 0;
            memory_test.state.current_region++;
            if (memory_test.state.current_region >= memory_test.config.region_count) {
                memory_test.state.current_region = 0;
            }
        }
        
        timer_start(&memory_test.state.test_timer, 
                   memory_test.config.test_interval_ms);
    }
    
    exit_critical(&memory_test.critical);
}

MemoryTestResult Memory_Test_RunTest(MemoryTestType test, uint32_t region_index) {
    if (!memory_test.state.initialized || 
        region_index >= memory_test.config.region_count) {
        return MEM_TEST_FAILED_TIMEOUT;
    }
    
    MemoryTestResult result;
    
    enter_critical(&memory_test.critical);
    
    switch (test) {
        case MEM_TEST_MARCH_C:
            result = march_c_test(region_index);
            break;
        case MEM_TEST_CHECKERBOARD:
            result = checkerboard_test(region_index);
            break;
        case MEM_TEST_WALKING_1:
            result = walking_1_test(region_index);
            break;
        case MEM_TEST_WALKING_0:
            result = walking_0_test(region_index);
            break;
        case MEM_TEST_ADDRESS_FAULT:
            result = address_fault_test(region_index);
            break;
        case MEM_TEST_FLASH_CRC:
            result = flash_crc_test(region_index);
            break;
        case MEM_TEST_RAM_PATTERN:
            result = ram_pattern_test(region_index);
            break;
        default:
            result = MEM_TEST_FAILED_TIMEOUT;
            break;
    }
    
    if (result != MEM_TEST_OK) {
        memory_test.state.total_errors++;
        memory_test.state.last_results[region_index] = result;
        
        if (memory_test.config.error_callback) {
            memory_test.config.error_callback(test,
                                            result,
                                            memory_test.config.regions[region_index].start_address);
        }
    }
    
    exit_critical(&memory_test.critical);
    return result;
}

bool Memory_Test_IsRegionHealthy(uint32_t region_index) {
    if (!memory_test.state.initialized || 
        region_index >= memory_test.config.region_count) {
        return false;
    }
    
    return memory_test.state.last_results[region_index] == MEM_TEST_OK;
}

uint32_t Memory_Test_GetErrorCount(void) {
    return memory_test.state.total_errors;
}

void Memory_Test_ResetErrorCount(void) {
    enter_critical(&memory_test.critical);
    memory_test.state.total_errors = 0;
    memset(memory_test.state.last_results, 0, 
           memory_test.config.region_count * sizeof(MemoryTestResult));
    exit_critical(&memory_test.critical);
}

void Memory_Test_GetStatus(MemoryTestResult* results, uint32_t* count) {
    if (!results || !count || !memory_test.state.initialized) return;
    
    enter_critical(&memory_test.critical);
    memcpy(results, memory_test.state.last_results, 
           memory_test.config.region_count * sizeof(MemoryTestResult));
    *count = memory_test.config.region_count;
    exit_critical(&memory_test.critical);
}

bool Memory_Test_VerifyRegion(uint32_t region_index, uint32_t* failed_address) {
    if (!memory_test.state.initialized || 
        region_index >= memory_test.config.region_count) {
        return false;
    }
    
    const MemoryRegionConfig* region = &memory_test.config.regions[region_index];
    bool result = true;
    
    enter_critical(&memory_test.critical);
    
    // Run all applicable tests
    MemoryTestResult test_result;
    
    test_result = march_c_test(region_index);
    if (test_result != MEM_TEST_OK) {
        result = false;
        goto exit;
    }
    
    test_result = checkerboard_test(region_index);
    if (test_result != MEM_TEST_OK) {
        result = false;
        goto exit;
    }
    
    test_result = walking_1_test(region_index);
    if (test_result != MEM_TEST_OK) {
        result = false;
        goto exit;
    }
    
    test_result = walking_0_test(region_index);
    if (test_result != MEM_TEST_OK) {
        result = false;
        goto exit;
    }
    
    test_result = address_fault_test(region_index);
    if (test_result != MEM_TEST_OK) {
        result = false;
        goto exit;
    }
    
    if (region->type == MEM_REGION_FLASH || region->type == MEM_REGION_ROM) {
        test_result = flash_crc_test(region_index);
        if (test_result != MEM_TEST_OK) {
            result = false;
            goto exit;
        }
    }
    
    if (region->type == MEM_REGION_RAM) {
        test_result = ram_pattern_test(region_index);
        if (test_result != MEM_TEST_OK) {
            result = false;
            goto exit;
        }
    }
    
exit:
    if (!result && failed_address) {
        *failed_address = region->start_address;
    }
    
    exit_critical(&memory_test.critical);
    return result;
} 