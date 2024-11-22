#ifndef CANT_CRC_H
#define CANT_CRC_H

#include <stdint.h>
#include <stddef.h>

// CRC API
uint8_t calculate_crc8(const uint8_t* data, size_t length);
uint16_t calculate_crc16(const uint8_t* data, size_t length);
uint32_t calculate_crc32(const uint8_t* data, size_t length);
uint64_t calculate_crc64(const uint8_t* data, size_t length);

// CRC Configuration
void crc_init(void);
void crc_set_polynomial(uint32_t polynomial);
void crc_set_initial_value(uint32_t initial);
void crc_set_final_xor(uint32_t final_xor);

#endif // CANT_CRC_H 