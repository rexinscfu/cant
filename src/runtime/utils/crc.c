#include "crc.h"
#include "../os/critical.h"

// CRC configuration state
static struct {
    uint32_t polynomial;
    uint32_t initial_value;
    uint32_t final_xor;
    CriticalSection critical;
    bool initialized;
} crc_state = {
    .polynomial = 0x04C11DB7,    // Default CRC-32 polynomial
    .initial_value = 0xFFFFFFFF, // Default initial value
    .final_xor = 0xFFFFFFFF,    // Default final XOR value
    .initialized = false
};

// Pre-calculated CRC tables
static uint8_t crc8_table[256];
static uint16_t crc16_table[256];
static uint32_t crc32_table[256];
static uint64_t crc64_table[256];

// Initialize CRC tables
static void init_crc_tables(void) {
    // CRC-8 table generation
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x80) ? 0x07 : 0);
        }
        crc8_table[i] = crc;
    }
    
    // CRC-16 table generation
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
        }
        crc16_table[i] = crc;
    }
    
    // CRC-32 table generation
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? crc_state.polynomial : 0);
        }
        crc32_table[i] = crc;
    }
    
    // CRC-64 table generation
    const uint64_t poly64 = 0x42F0E1EBA9EA3693;
    for (int i = 0; i < 256; i++) {
        uint64_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? poly64 : 0);
        }
        crc64_table[i] = crc;
    }
}

void crc_init(void) {
    if (crc_state.initialized) return;
    
    init_critical(&crc_state.critical);
    init_crc_tables();
    
    crc_state.initialized = true;
}

void crc_set_polynomial(uint32_t polynomial) {
    enter_critical(&crc_state.critical);
    crc_state.polynomial = polynomial;
    init_crc_tables();  // Regenerate tables with new polynomial
    exit_critical(&crc_state.critical);
}

void crc_set_initial_value(uint32_t initial) {
    enter_critical(&crc_state.critical);
    crc_state.initial_value = initial;
    exit_critical(&crc_state.critical);
}

void crc_set_final_xor(uint32_t final_xor) {
    enter_critical(&crc_state.critical);
    crc_state.final_xor = final_xor;
    exit_critical(&crc_state.critical);
}

uint8_t calculate_crc8(const uint8_t* data, size_t length) {
    if (!data || !length) return 0;
    
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

uint16_t calculate_crc16(const uint8_t* data, size_t length) {
    if (!data || !length) return 0;
    
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    if (!data || !length) return 0;
    
    uint32_t crc = crc_state.initial_value;
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ data[i]];
    }
    return crc ^ crc_state.final_xor;
}

uint64_t calculate_crc64(const uint8_t* data, size_t length) {
    if (!data || !length) return 0;
    
    uint64_t crc = 0xFFFFFFFFFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc64_table[(crc & 0xFF) ^ data[i]];
    }
    return crc;
} 