#include "diag_data.h"
#include "../../hw/ecu.h"
#include "../../hw/battery.h"
#include "../../hw/sensors.h"
#include "../../hw/network.h"
#include "../../os/critical.h"
#include "../../utils/nvram.h"
#include <string.h>

// Vehicle information storage
typedef struct {
    char vin[17];
    char vehicle_model[32];
    char system_name[32];
    char repair_shop_code[10];
    char programming_date[10];
} VehicleInfo;

static VehicleInfo vehicle_info;

// ECU information storage
typedef struct {
    char serial_number[16];
    uint16_t hw_version;
    uint16_t sw_version;
    char mfg_date[10];
    uint16_t supplier_id;
} ECUInfo;

static ECUInfo ecu_info;

// System status storage
typedef struct {
    float battery_voltage;
    float system_voltage;
    uint16_t engine_speed;
    float vehicle_speed;
    float engine_temp;
    float ambient_temp;
} SystemStatus;

static SystemStatus system_status;

// Diagnostic data storage
typedef struct {
    uint32_t total_distance;
    uint32_t total_runtime;
    uint16_t last_error_code;
    uint16_t error_count;
    uint16_t boot_count;
} DiagnosticData;

static DiagnosticData diagnostic_data;

// Configuration storage
typedef struct {
    uint32_t network_config;
    uint32_t can_baudrate;
    uint8_t node_address;
    uint16_t security_config;
    uint16_t routine_config;
} ConfigurationData;

static ConfigurationData config_data;

// Vehicle Information Handlers
bool read_vehicle_info(uint16_t did, uint8_t* data, uint16_t* length) {
    enter_critical();
    
    bool success = true;
    switch (did) {
        case DID_VIN:
            memcpy(data, vehicle_info.vin, 17);
            *length = 17;
            break;
            
        case DID_VEHICLE_MODEL:
            memcpy(data, vehicle_info.vehicle_model, strlen(vehicle_info.vehicle_model));
            *length = strlen(vehicle_info.vehicle_model);
            break;
            
        case DID_SYSTEM_NAME:
            memcpy(data, vehicle_info.system_name, strlen(vehicle_info.system_name));
            *length = strlen(vehicle_info.system_name);
            break;
            
        case DID_REPAIR_SHOP_CODE:
            memcpy(data, vehicle_info.repair_shop_code, strlen(vehicle_info.repair_shop_code));
            *length = strlen(vehicle_info.repair_shop_code);
            break;
            
        case DID_PROGRAMMING_DATE:
            memcpy(data, vehicle_info.programming_date, strlen(vehicle_info.programming_date));
            *length = strlen(vehicle_info.programming_date);
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

bool write_vehicle_info(uint16_t did, const uint8_t* data, uint16_t length) {
    enter_critical();
    
    bool success = true;
    switch (did) {
        case DID_VIN:
            if (length == 17) {
                memcpy(vehicle_info.vin, data, length);
                NVRAM_Write(NVRAM_ADDR_VIN, data, length);
            } else {
                success = false;
            }
            break;
            
        case DID_VEHICLE_MODEL:
            if (length <= sizeof(vehicle_info.vehicle_model)) {
                memcpy(vehicle_info.vehicle_model, data, length);
                vehicle_info.vehicle_model[length] = '\0';
                NVRAM_Write(NVRAM_ADDR_VEHICLE_MODEL, data, length + 1);
            } else {
                success = false;
            }
            break;
            
        case DID_SYSTEM_NAME:
            if (length <= sizeof(vehicle_info.system_name)) {
                memcpy(vehicle_info.system_name, data, length);
                vehicle_info.system_name[length] = '\0';
                NVRAM_Write(NVRAM_ADDR_SYSTEM_NAME, data, length + 1);
            } else {
                success = false;
            }
            break;
            
        case DID_REPAIR_SHOP_CODE:
            if (length <= sizeof(vehicle_info.repair_shop_code)) {
                memcpy(vehicle_info.repair_shop_code, data, length);
                vehicle_info.repair_shop_code[length] = '\0';
                NVRAM_Write(NVRAM_ADDR_REPAIR_SHOP, data, length + 1);
            } else {
                success = false;
            }
            break;
            
        case DID_PROGRAMMING_DATE:
            if (length <= sizeof(vehicle_info.programming_date)) {
                memcpy(vehicle_info.programming_date, data, length);
                vehicle_info.programming_date[length] = '\0';
                NVRAM_Write(NVRAM_ADDR_PROG_DATE, data, length + 1);
            } else {
                success = false;
            }
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

// ECU Information Handlers
bool read_ecu_info(uint16_t did, uint8_t* data, uint16_t* length) {
    enter_critical();
    
    bool success = true;
    switch (did) {
        case DID_ECU_SERIAL:
            memcpy(data, ecu_info.serial_number, strlen(ecu_info.serial_number));
            *length = strlen(ecu_info.serial_number);
            break;
            
        case DID_ECU_HW_VERSION:
            memcpy(data, &ecu_info.hw_version, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        case DID_ECU_SW_VERSION:
            memcpy(data, &ecu_info.sw_version, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        case DID_ECU_MFG_DATE:
            memcpy(data, ecu_info.mfg_date, strlen(ecu_info.mfg_date));
            *length = strlen(ecu_info.mfg_date);
            break;
            
        case DID_ECU_SUPPLIER_ID:
            memcpy(data, &ecu_info.supplier_id, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

// System Status Handlers
bool read_system_status(uint16_t did, uint8_t* data, uint16_t* length) {
    enter_critical();
    
    // Update real-time values before reading
    system_status.battery_voltage = Battery_GetVoltage();
    system_status.system_voltage = ECU_GetSystemVoltage();
    system_status.engine_speed = ECU_GetEngineSpeed();
    system_status.vehicle_speed = ECU_GetVehicleSpeed();
    system_status.engine_temp = Sensor_GetTemperature(SENSOR_ENGINE_TEMP);
    system_status.ambient_temp = Sensor_GetTemperature(SENSOR_AMBIENT_TEMP);
    
    bool success = true;
    switch (did) {
        case DID_BATTERY_VOLTAGE:
            memcpy(data, &system_status.battery_voltage, sizeof(float));
            *length = sizeof(float);
            break;
            
        case DID_SYSTEM_VOLTAGE:
            memcpy(data, &system_status.system_voltage, sizeof(float));
            *length = sizeof(float);
            break;
            
        case DID_ENGINE_SPEED:
            memcpy(data, &system_status.engine_speed, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        case DID_VEHICLE_SPEED:
            memcpy(data, &system_status.vehicle_speed, sizeof(float));
            *length = sizeof(float);
            break;
            
        case DID_ENGINE_TEMP:
            memcpy(data, &system_status.engine_temp, sizeof(float));
            *length = sizeof(float);
            break;
            
        case DID_AMBIENT_TEMP:
            memcpy(data, &system_status.ambient_temp, sizeof(float));
            *length = sizeof(float);
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

// Diagnostic Data Handlers
bool read_diagnostic_data(uint16_t did, uint8_t* data, uint16_t* length) {
    enter_critical();
    
    // Update diagnostic counters
    diagnostic_data.total_runtime = ECU_GetTotalRuntime();
    diagnostic_data.error_count = ECU_GetErrorCount();
    
    bool success = true;
    switch (did) {
        case DID_TOTAL_DISTANCE:
            memcpy(data, &diagnostic_data.total_distance, sizeof(uint32_t));
            *length = sizeof(uint32_t);
            break;
            
        case DID_TOTAL_RUNTIME:
            memcpy(data, &diagnostic_data.total_runtime, sizeof(uint32_t));
            *length = sizeof(uint32_t);
            break;
            
        case DID_LAST_ERROR_CODE:
            memcpy(data, &diagnostic_data.last_error_code, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        case DID_ERROR_COUNT:
            memcpy(data, &diagnostic_data.error_count, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        case DID_BOOT_COUNT:
            memcpy(data, &diagnostic_data.boot_count, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

// Configuration Handlers
bool read_configuration(uint16_t did, uint8_t* data, uint16_t* length) {
    enter_critical();
    
    bool success = true;
    switch (did) {
        case DID_NETWORK_CONFIG:
            memcpy(data, &config_data.network_config, sizeof(uint32_t));
            *length = sizeof(uint32_t);
            break;
            
        case DID_CAN_BAUDRATE:
            memcpy(data, &config_data.can_baudrate, sizeof(uint32_t));
            *length = sizeof(uint32_t);
            break;
            
        case DID_NODE_ADDRESS:
            memcpy(data, &config_data.node_address, sizeof(uint8_t));
            *length = sizeof(uint8_t);
            break;
            
        case DID_SECURITY_CONFIG:
            memcpy(data, &config_data.security_config, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        case DID_ROUTINE_CONFIG:
            memcpy(data, &config_data.routine_config, sizeof(uint16_t));
            *length = sizeof(uint16_t);
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

bool write_configuration(uint16_t did, const uint8_t* data, uint16_t length) {
    enter_critical();
    
    bool success = true;
    switch (did) {
        case DID_NETWORK_CONFIG:
            if (length == sizeof(uint32_t)) {
                memcpy(&config_data.network_config, data, length);
                NVRAM_Write(NVRAM_ADDR_NETWORK_CONFIG, data, length);
                Network_ApplyConfig(config_data.network_config);
            } else {
                success = false;
            }
            break;
            
        case DID_CAN_BAUDRATE:
            if (length == sizeof(uint32_t)) {
                memcpy(&config_data.can_baudrate, data, length);
                NVRAM_Write(NVRAM_ADDR_CAN_BAUDRATE, data, length);
                Network_SetBaudrate(config_data.can_baudrate);
            } else {
                success = false;
            }
            break;
            
        case DID_NODE_ADDRESS:
            if (length == sizeof(uint8_t)) {
                memcpy(&config_data.node_address, data, length);
                NVRAM_Write(NVRAM_ADDR_NODE_ADDRESS, data, length);
                Network_SetNodeAddress(config_data.node_address);
            } else {
                success = false;
            }
            break;
            
        case DID_SECURITY_CONFIG:
            if (length == sizeof(uint16_t)) {
                memcpy(&config_data.security_config, data, length);
                NVRAM_Write(NVRAM_ADDR_SECURITY_CONFIG, data, length);
                Security_Manager_UpdateConfig(config_data.security_config);
            } else {
                success = false;
            }
            break;
            
        case DID_ROUTINE_CONFIG:
            if (length == sizeof(uint16_t)) {
                memcpy(&config_data.routine_config, data, length);
                NVRAM_Write(NVRAM_ADDR_ROUTINE_CONFIG, data, length);
                Routine_Manager_UpdateConfig(config_data.routine_config);
            } else {
                success = false;
            }
            break;
            
        default:
            success = false;
            break;
    }
    
    exit_critical();
    return success;
}

// Initialize all data structures
void Diag_Data_Init(void) {
    // Load vehicle info from NVRAM
    NVRAM_Read(NVRAM_ADDR_VIN, vehicle_info.vin, sizeof(vehicle_info.vin));
    NVRAM_Read(NVRAM_ADDR_VEHICLE_MODEL, vehicle_info.vehicle_model, sizeof(vehicle_info.vehicle_model));
    NVRAM_Read(NVRAM_ADDR_SYSTEM_NAME, vehicle_info.system_name, sizeof(vehicle_info.system_name));
    NVRAM_Read(NVRAM_ADDR_REPAIR_SHOP, vehicle_info.repair_shop_code, sizeof(vehicle_info.repair_shop_code));
    NVRAM_Read(NVRAM_ADDR_PROG_DATE, vehicle_info.programming_date, sizeof(vehicle_info.programming_date));

    // Load ECU info
    ECU_GetSerialNumber(ecu_info.serial_number);
    ecu_info.hw_version = ECU_GetHardwareVersion();
    ecu_info.sw_version = ECU_GetSoftwareVersion();
    ECU_GetManufacturingDate(ecu_info.mfg_date);
    ecu_info.supplier_id = ECU_GetSupplierId();

    // Load configuration data from NVRAM
    NVRAM_Read(NVRAM_ADDR_NETWORK_CONFIG, &config_data.network_config, sizeof(uint32_t));
    NVRAM_Read(NVRAM_ADDR_CAN_BAUDRATE, &config_data.can_baudrate, sizeof(uint32_t));
    NVRAM_Read(NVRAM_ADDR_NODE_ADDRESS, &config_data.node_address, sizeof(uint8_t));
    NVRAM_Read(NVRAM_ADDR_SECURITY_CONFIG, &config_data.security_config, sizeof(uint16_t));
    NVRAM_Read(NVRAM_ADDR_ROUTINE_CONFIG, &config_data.routine_config, sizeof(uint16_t));

    // Initialize diagnostic data
    diagnostic_data.total_distance = ECU_GetTotalDistance();
    diagnostic_data.total_runtime = ECU_GetTotalRuntime();
    diagnostic_data.last_error_code = ECU_GetLastErrorCode();
    diagnostic_data.error_count = ECU_GetErrorCount();
    diagnostic_data.boot_count = ECU_GetBootCount();
}