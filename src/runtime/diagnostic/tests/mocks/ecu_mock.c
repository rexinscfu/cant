#include "ecu_mock.h"
#include <string.h>

static struct {
    char serial_number[16];
    uint16_t hw_version;
    uint16_t sw_version;
    char mfg_date[10];
    uint16_t supplier_id;
    float system_voltage;
    uint16_t engine_speed;
    float vehicle_speed;
    uint32_t total_distance;
    uint32_t total_runtime;
    uint16_t error_count;
    uint16_t last_error_code;
    uint16_t boot_count;
} ecu_mock_data;

void ECU_Mock_Init(void) {
    memset(&ecu_mock_data, 0, sizeof(ecu_mock_data));
    strcpy(ecu_mock_data.serial_number, "TEST123456789");
    strcpy(ecu_mock_data.mfg_date, "2024-03-15");
    ecu_mock_data.hw_version = 0x0100;
    ecu_mock_data.sw_version = 0x0100;
    ecu_mock_data.supplier_id = 0x1234;
}

void ECU_Mock_Deinit(void) {
    memset(&ecu_mock_data, 0, sizeof(ecu_mock_data));
}

// Setter implementations
void ECU_Mock_SetSerialNumber(const char* serial) {
    strncpy(ecu_mock_data.serial_number, serial, sizeof(ecu_mock_data.serial_number) - 1);
}

void ECU_Mock_SetHardwareVersion(uint16_t version) {
    ecu_mock_data.hw_version = version;
}


// Getter implementations
const char* ECU_Mock_GetSerialNumber(void) {
    return ecu_mock_data.serial_number;
}

uint16_t ECU_Mock_GetHardwareVersion(void) {
    return ecu_mock_data.hw_version;
}

