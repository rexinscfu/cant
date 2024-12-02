#include "sys_monitor.h"
#include "../memory/mem_pool.h"
#include "../network/buffer_manager.h"
#include "../../hardware/watchdog.h"
#include <string.h>

#define HISTORY_SIZE 60  // 1 minute of history at 1Hz

typedef struct {
    uint32_t mem_usage;
    uint32_t buf_usage;
    uint32_t msg_count;
    uint32_t errors;
    uint32_t timestamp;
} SystemSnapshot;

static SystemSnapshot history[HISTORY_SIZE];
static uint32_t history_index = 0;
static SystemStats current_stats;

void SysMonitor_Init(void) {
    memset(history, 0, sizeof(history));
    memset(&current_stats, 0, sizeof(SystemStats));
    history_index = 0;
}

void SysMonitor_Update(void) {
    SystemSnapshot* snap = &history[history_index];
    
    snap->mem_usage = POOL_NUM_BLOCKS - MemPool_GetFreeBlocks();
    snap->buf_usage = BufferManager_GetUsage();
    snap->msg_count = get_msg_count();
    snap->errors = WATCHDOG_GetResetCount();
    snap->timestamp = TIMER_GetMs();
    
    history_index = (history_index + 1) % HISTORY_SIZE;
    
    update_stats();
}

static void update_stats(void) {
    uint32_t total_mem = 0;
    uint32_t total_buf = 0;
    uint32_t total_msg = 0;
    
    for(int i = 0; i < HISTORY_SIZE; i++) {
        total_mem += history[i].mem_usage;
        total_buf += history[i].buf_usage;
        total_msg += history[i].msg_count;
    }
    
    current_stats.avg_mem_usage = total_mem / HISTORY_SIZE;
    current_stats.avg_buf_usage = total_buf / HISTORY_SIZE;
    current_stats.msg_rate = total_msg / 60;  // per minute
    current_stats.last_error = history[history_index].errors;
}

void SysMonitor_GetStats(SystemStats* stats) {
    if(stats) {
        memcpy(stats, &current_stats, sizeof(SystemStats));
    }
}

static uint32_t warning_count = 0; 