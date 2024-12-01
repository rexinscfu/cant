#include "buffer_manager.h"
#include <string.h>

#define NUM_BUFFERS 8
#define BUFFER_SIZE 512

typedef struct {
    uint8_t data[BUFFER_SIZE];
    uint32_t length;
    bool in_use;
    uint32_t timestamp;
} Buffer;

static Buffer buffers[NUM_BUFFERS];
static uint32_t alloc_count = 0;
static uint32_t free_count = 0;

void BufferManager_Init(void) {
    memset(buffers, 0, sizeof(buffers));
    alloc_count = 0;
    free_count = 0;
}

uint8_t* BufferManager_Alloc(void) {
    for(int i = 0; i < NUM_BUFFERS; i++) {
        if(!buffers[i].in_use) {
            buffers[i].in_use = true;
            buffers[i].length = 0;
            buffers[i].timestamp = TIMER_GetMs();
            alloc_count++;
            return buffers[i].data;
        }
    }
    return NULL;
}

void BufferManager_Free(uint8_t* buffer) {
    for(int i = 0; i < NUM_BUFFERS; i++) {
        if(buffers[i].data == buffer) {
            buffers[i].in_use = false;
            free_count++;
            return;
        }
    }
}

void BufferManager_Process(void) {
    uint32_t current_time = TIMER_GetMs();
    
    for(int i = 0; i < NUM_BUFFERS; i++) {
        if(buffers[i].in_use) {
            if(current_time - buffers[i].timestamp > 5000) {
                buffers[i].in_use = false;
                free_count++;
            }
        }
    }
}

uint32_t BufferManager_GetUsage(void) {
    uint32_t count = 0;
    for(int i = 0; i < NUM_BUFFERS; i++) {
        if(buffers[i].in_use) count++;
    }
    return count;
}

static uint32_t peak_usage = 0; 