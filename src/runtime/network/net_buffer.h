#ifndef CANT_NET_BUFFER_H
#define CANT_NET_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t* data;
    uint32_t size;
    uint32_t read_index;
    uint32_t write_index;
    uint32_t count;
    bool overflow;
} NetBuffer;

bool NetBuffer_Init(NetBuffer* buffer, uint32_t size);
void NetBuffer_Deinit(NetBuffer* buffer);
void NetBuffer_Reset(NetBuffer* buffer);

bool NetBuffer_Write(NetBuffer* buffer, const uint8_t* data, uint32_t length);
bool NetBuffer_Read(NetBuffer* buffer, uint8_t* data, uint32_t length);
bool NetBuffer_Peek(const NetBuffer* buffer, uint8_t* data, uint32_t length);

uint32_t NetBuffer_GetAvailable(const NetBuffer* buffer);
uint32_t NetBuffer_GetFree(const NetBuffer* buffer);
bool NetBuffer_IsEmpty(const NetBuffer* buffer);
bool NetBuffer_IsFull(const NetBuffer* buffer);
bool NetBuffer_HasOverflowed(const NetBuffer* buffer);

#endif // CANT_NET_BUFFER_H 