#include "net_buffer.h"
#include "../diagnostic/logging/diag_logger.h"
#include "../diagnostic/os/critical.h"
#include <string.h>
#include <stdlib.h>

bool NetBuffer_Init(NetBuffer* buffer, uint32_t size) {
    if (!buffer || size == 0) {
        return false;
    }

    buffer->data = (uint8_t*)malloc(size);
    if (!buffer->data) {
        Logger_Log(LOG_LEVEL_ERROR, "NETBUF", "Failed to allocate buffer of size %u", size);
        return false;
    }

    buffer->size = size;
    NetBuffer_Reset(buffer);
    
    Logger_Log(LOG_LEVEL_DEBUG, "NETBUF", "Initialized buffer of size %u", size);
    return true;
}

void NetBuffer_Deinit(NetBuffer* buffer) {
    if (!buffer) {
        return;
    }

    if (buffer->data) {
        free(buffer->data);
        buffer->data = NULL;
    }

    buffer->size = 0;
    buffer->read_index = 0;
    buffer->write_index = 0;
    buffer->count = 0;
    buffer->overflow = false;
}

void NetBuffer_Reset(NetBuffer* buffer) {
    if (!buffer) {
        return;
    }

    buffer->read_index = 0;
    buffer->write_index = 0;
    buffer->count = 0;
    buffer->overflow = false;
    
    if (buffer->data) {
        memset(buffer->data, 0, buffer->size);
    }
}

bool NetBuffer_Write(NetBuffer* buffer, const uint8_t* data, uint32_t length) {
    if (!buffer || !buffer->data || !data || length == 0) {
        return false;
    }

    enter_critical();

    if (length > NetBuffer_GetFree(buffer)) {
        buffer->overflow = true;
        exit_critical();
        Logger_Log(LOG_LEVEL_WARNING, "NETBUF", "Buffer overflow detected");
        return false;
    }

    // Handle wrap-around
    uint32_t first_chunk = buffer->size - buffer->write_index;
    if (length <= first_chunk) {
        memcpy(buffer->data + buffer->write_index, data, length);
        buffer->write_index += length;
        if (buffer->write_index >= buffer->size) {
            buffer->write_index = 0;
        }
    } else {
        // Copy first part up to end of buffer
        memcpy(buffer->data + buffer->write_index, data, first_chunk);
        // Copy remaining part at start of buffer
        memcpy(buffer->data, data + first_chunk, length - first_chunk);
        buffer->write_index = length - first_chunk;
    }

    buffer->count += length;

    exit_critical();
    return true;
}

bool NetBuffer_Read(NetBuffer* buffer, uint8_t* data, uint32_t length) {
    if (!buffer || !buffer->data || !data || length == 0) {
        return false;
    }

    enter_critical();

    if (length > buffer->count) {
        exit_critical();
        return false;
    }

    // Handle wrap-around
    uint32_t first_chunk = buffer->size - buffer->read_index;
    if (length <= first_chunk) {
        memcpy(data, buffer->data + buffer->read_index, length);
        buffer->read_index += length;
        if (buffer->read_index >= buffer->size) {
            buffer->read_index = 0;
        }
    } else {
        // Copy first part up to end of buffer
        memcpy(data, buffer->data + buffer->read_index, first_chunk);
        // Copy remaining part from start of buffer
        memcpy(data + first_chunk, buffer->data, length - first_chunk);
        buffer->read_index = length - first_chunk;
    }

    buffer->count -= length;

    exit_critical();
    return true;
}

bool NetBuffer_Peek(const NetBuffer* buffer, uint8_t* data, uint32_t length) {
    if (!buffer || !buffer->data || !data || length == 0) {
        return false;
    }

    enter_critical();

    if (length > buffer->count) {
        exit_critical();
        return false;
    }

    uint32_t read_index = buffer->read_index;
    uint32_t first_chunk = buffer->size - read_index;
    if (length <= first_chunk) {
        memcpy(data, buffer->data + read_index, length);
    } else {
        memcpy(data, buffer->data + read_index, first_chunk);
        memcpy(data + first_chunk, buffer->data, length - first_chunk);
    }

    exit_critical();
    return true;
}

uint32_t NetBuffer_GetAvailable(const NetBuffer* buffer) {
    if (!buffer) {
        return 0;
    }
    return buffer->count;
}

uint32_t NetBuffer_GetFree(const NetBuffer* buffer) {
    if (!buffer) {
        return 0;
    }
    return buffer->size - buffer->count;
}

bool NetBuffer_IsEmpty(const NetBuffer* buffer) {
    if (!buffer) {
        return true;
    }
    return buffer->count == 0;
}

bool NetBuffer_IsFull(const NetBuffer* buffer) {
    if (!buffer) {
        return true;
    }
    return buffer->count >= buffer->size;
}

bool NetBuffer_HasOverflowed(const NetBuffer* buffer) {
    if (!buffer) {
        return false;
    }
    return buffer->overflow;
} 