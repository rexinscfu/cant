#include "memory_manager.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

// Internal state structure
typedef struct {
    MemoryManagerConfig config;
    struct {
        bool is_busy;
        uint32_t current_address;
        uint32_t remaining_bytes;
        MemoryResult last_result;
        Timer operation_timer;
        bool initialized;
    } state;
    CriticalSection critical;
} MemoryManager;

static MemoryManager memory_manager;

static const MemoryAreaDef* find_memory_area(uint32_t address) {
    for (uint32_t i = 0; i < memory_manager.config.area_count; i++) {
        const MemoryAreaDef* area = &memory_manager.config.memory_areas[i];
        if (address >= area->start_address && 
            address < (area->start_address + area->size)) {
            return area;
        }
    }
    return NULL;
}

static bool validate_address_range(uint32_t address, uint32_t length) {
    const MemoryAreaDef* start_area = find_memory_area(address);
    const MemoryAreaDef* end_area = find_memory_area(address + length - 1);
    
    return (start_area != NULL && end_area != NULL && start_area == end_area);
}

static bool check_access_rights(uint32_t address, uint32_t length, uint8_t required_rights) {
    const MemoryAreaDef* area = find_memory_area(address);
    if (!area) return false;
    
    return ((area->access_rights & required_rights) == required_rights);
}

static MemoryResult platform_read_memory(uint32_t address, uint8_t* data, uint32_t length) {
    const MemoryAreaDef* area = find_memory_area(address);
    if (!area) return MEMORY_RESULT_INVALID_ADDRESS;

    switch (area->type) {
        case MEMORY_AREA_RAM:
            memcpy(data, (void*)address, length);
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_FLASH:
            // Platform specific flash read implementation
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_EEPROM:
            // Platform specific EEPROM read implementation
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_ROM:
            memcpy(data, (const void*)address, length);
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_MIRROR:
            // Handle mirrored memory access
            return MEMORY_RESULT_SUCCESS;

        default:
            return MEMORY_RESULT_ERROR;
    }
}

static MemoryResult platform_write_memory(uint32_t address, const uint8_t* data, uint32_t length) {
    const MemoryAreaDef* area = find_memory_area(address);
    if (!area) return MEMORY_RESULT_INVALID_ADDRESS;

    switch (area->type) {
        case MEMORY_AREA_RAM:
            memcpy((void*)address, data, length);
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_FLASH:
            // Platform specific flash write implementation
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_EEPROM:
            // Platform specific EEPROM write implementation
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_MIRROR:
            // Handle mirrored memory access
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_ROM:
            return MEMORY_RESULT_ACCESS_DENIED;

        default:
            return MEMORY_RESULT_ERROR;
    }
}

static MemoryResult platform_erase_memory(uint32_t address, uint32_t length) {
    const MemoryAreaDef* area = find_memory_area(address);
    if (!area) return MEMORY_RESULT_INVALID_ADDRESS;

    switch (area->type) {
        case MEMORY_AREA_FLASH:
            // Platform specific flash erase implementation
            return MEMORY_RESULT_SUCCESS;

        case MEMORY_AREA_EEPROM:
            // Platform specific EEPROM erase implementation
            return MEMORY_RESULT_SUCCESS;

        default:
            return MEMORY_RESULT_ACCESS_DENIED;
    }
}

bool Memory_Manager_Init(const MemoryManagerConfig* config) {
    if (!config || !config->memory_areas || config->area_count == 0) {
        return false;
    }

    enter_critical(&memory_manager.critical);

    memcpy(&memory_manager.config, config, sizeof(MemoryManagerConfig));
    
    memory_manager.state.is_busy = false;
    memory_manager.state.current_address = 0;
    memory_manager.state.remaining_bytes = 0;
    memory_manager.state.last_result = MEMORY_RESULT_SUCCESS;
    
    timer_init();
    init_critical(&memory_manager.critical);
    
    memory_manager.state.initialized = true;

    exit_critical(&memory_manager.critical);
    return true;
}

void Memory_Manager_DeInit(void) {
    enter_critical(&memory_manager.critical);
    memset(&memory_manager, 0, sizeof(MemoryManager));
    exit_critical(&memory_manager.critical);
}

MemoryResult Memory_Manager_Read(uint32_t address, uint8_t* data, uint32_t length) {
    if (!memory_manager.state.initialized || !data || length == 0) {
        return MEMORY_RESULT_ERROR;
    }

    enter_critical(&memory_manager.critical);

    if (memory_manager.state.is_busy) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_BUSY;
    }

    if (!validate_address_range(address, length)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_INVALID_ADDRESS;
    }

    if (!check_access_rights(address, length, MEMORY_ACCESS_READ)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_ACCESS_DENIED;
    }

    memory_manager.state.is_busy = true;
    memory_manager.state.current_address = address;
    memory_manager.state.remaining_bytes = length;

    MemoryResult result = platform_read_memory(address, data, length);
    
    memory_manager.state.is_busy = false;
    memory_manager.state.last_result = result;

    if (memory_manager.config.operation_complete_callback) {
        memory_manager.config.operation_complete_callback(result);
    }

    exit_critical(&memory_manager.critical);
    return result;
}

MemoryResult Memory_Manager_Write(uint32_t address, const uint8_t* data, uint32_t length) {
    if (!memory_manager.state.initialized || !data || length == 0) {
        return MEMORY_RESULT_ERROR;
    }

    enter_critical(&memory_manager.critical);

    if (memory_manager.state.is_busy) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_BUSY;
    }

    if (!validate_address_range(address, length)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_INVALID_ADDRESS;
    }

    if (!check_access_rights(address, length, MEMORY_ACCESS_WRITE)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_ACCESS_DENIED;
    }

    memory_manager.state.is_busy = true;
    memory_manager.state.current_address = address;
    memory_manager.state.remaining_bytes = length;
    timer_start(&memory_manager.state.operation_timer, memory_manager.config.write_timeout_ms);

    MemoryResult result = platform_write_memory(address, data, length);
    
    memory_manager.state.is_busy = false;
    memory_manager.state.last_result = result;

    if (memory_manager.config.operation_complete_callback) {
        memory_manager.config.operation_complete_callback(result);
    }

    exit_critical(&memory_manager.critical);
    return result;
}

MemoryResult Memory_Manager_Erase(uint32_t address, uint32_t length) {
    if (!memory_manager.state.initialized || length == 0) {
        return MEMORY_RESULT_ERROR;
    }

    enter_critical(&memory_manager.critical);

    if (memory_manager.state.is_busy) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_BUSY;
    }

    if (!validate_address_range(address, length)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_INVALID_ADDRESS;
    }

    if (!check_access_rights(address, length, MEMORY_ACCESS_WRITE)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_ACCESS_DENIED;
    }

    memory_manager.state.is_busy = true;
    memory_manager.state.current_address = address;
    memory_manager.state.remaining_bytes = length;
    timer_start(&memory_manager.state.operation_timer, memory_manager.config.erase_timeout_ms);

    MemoryResult result = platform_erase_memory(address, length);
    
    memory_manager.state.is_busy = false;
    memory_manager.state.last_result = result;

    if (memory_manager.config.operation_complete_callback) {
        memory_manager.config.operation_complete_callback(result);
    }

    exit_critical(&memory_manager.critical);
    return result;
}

MemoryResult Memory_Manager_Verify(uint32_t address, const uint8_t* data, uint32_t length) {
    if (!memory_manager.state.initialized || !data || length == 0) {
        return MEMORY_RESULT_ERROR;
    }

    enter_critical(&memory_manager.critical);

    if (memory_manager.state.is_busy) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_BUSY;
    }

    if (!validate_address_range(address, length)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_INVALID_ADDRESS;
    }

    if (!check_access_rights(address, length, MEMORY_ACCESS_READ)) {
        exit_critical(&memory_manager.critical);
        return MEMORY_RESULT_ACCESS_DENIED;
    }

    uint8_t verify_buffer[256];
    uint32_t bytes_to_verify = length;
    uint32_t current_address = address;
    const uint8_t* compare_data = data;

    while (bytes_to_verify > 0) {
        uint32_t chunk_size = (bytes_to_verify > sizeof(verify_buffer)) ? 
                             sizeof(verify_buffer) : bytes_to_verify;

        MemoryResult read_result = platform_read_memory(current_address, 
                                                      verify_buffer, 
                                                      chunk_size);
        if (read_result != MEMORY_RESULT_SUCCESS) {
            memory_manager.state.last_result = read_result;
            exit_critical(&memory_manager.critical);
            return read_result;
        }

        if (memcmp(verify_buffer, compare_data, chunk_size) != 0) {
            memory_manager.state.last_result = MEMORY_RESULT_VERIFY_FAILED;
            exit_critical(&memory_manager.critical);
            return MEMORY_RESULT_VERIFY_FAILED;
        }

        bytes_to_verify -= chunk_size;
        current_address += chunk_size;
        compare_data += chunk_size;
    }

    exit_critical(&memory_manager.critical);
    return MEMORY_RESULT_SUCCESS;
}

bool Memory_Manager_IsAddressValid(uint32_t address, uint32_t length) {
    return validate_address_range(address, length);
}

bool Memory_Manager_HasAccess(uint32_t address, uint32_t length, uint8_t access_right) {
    return check_access_rights(address, length, access_right);
}

MemoryAreaType Memory_Manager_GetAreaType(uint32_t address) {
    const MemoryAreaDef* area = find_memory_area(address);
    return area ? area->type : MEMORY_AREA_RAM;
}

bool Memory_Manager_IsBusy(void) {
    return memory_manager.state.is_busy;
}

void Memory_Manager_AbortOperation(void) {
    enter_critical(&memory_manager.critical);
    memory_manager.state.is_busy = false;
    memory_manager.state.last_result = MEMORY_RESULT_ERROR;
    exit_critical(&memory_manager.critical);
}

MemoryResult Memory_Manager_GetLastResult(void) {
    return memory_manager.state.last_result;
}

uint32_t Memory_Manager_GetProgress(void) {
    if (!memory_manager.state.is_busy || memory_manager.state.remaining_bytes == 0) {
        return 100;
    }

    uint32_t total_bytes = memory_manager.state.remaining_bytes + 
                          (memory_manager.state.current_address - 
                           memory_manager.state.current_address);
    return ((total_bytes - memory_manager.state.remaining_bytes) * 100) / total_bytes;
}

