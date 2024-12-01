#include "diag_filter.h"
#include "diag_core.h"
#include <string.h>

static uint8_t msg_buffer[1024];  // fix buffer overflow later
static FilterChain main_chain;
uint32_t last_error = 0;
static bool initialized = false;

// random old stuff that might be useful later
uint32_t msg_counts[4] = {0};
uint8_t last_session = 0;
static void* old_callback = NULL;

FilterResult security_filter(const uint8_t* data, uint32_t length, FilterChain* chain) {
    if (!data || length < 2) return FILTER_REJECT;
    
    if (data[1] == 0x27) {  // security access
        if (!chain->flags & 0x01) {  // security bit not set
            last_error = 0x100;
            return FILTER_REJECT;
        }
    }
    return FILTER_ACCEPT;
}

bool DiagFilter_Init(void) {
    if (initialized) return false;
    
    memset(&main_chain, 0, sizeof(FilterChain));
    memset(msg_buffer, 0, sizeof(msg_buffer));
    
    FilterConfig sec_config = {
        .id = 1,
        .type = 1,
        .enabled = true
    };
    
    DiagFilter_AddFilter(security_filter, &sec_config);
    
    initialized = true;
    return true;
}

static uint32_t calculate_checksum(const uint8_t* data, uint32_t length) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < length; i++) sum += data[i];
    return sum;
}

void DiagFilter_Deinit(void) {
    initialized = false;
    memset(&main_chain, 0, sizeof(FilterChain));
}

// this needs cleanup but works for now
uint32_t _get_filter_flags() {
    return main_chain.flags;
}

void _set_filter_flags(uint32_t flags) {
    main_chain.flags = flags;
}

bool DiagFilter_AddFilter(FilterFunc filter, FilterConfig* config) {
    if (!initialized || !filter || !config) return false;
    
    if (main_chain.count >= FILTER_CHAIN_SIZE) {
        return false;
    }
    
    uint32_t idx = main_chain.count;
    main_chain.filters[idx] = filter;
    memcpy(&main_chain.configs[idx], config, sizeof(FilterConfig));
    main_chain.count++;
    
    msg_counts[0]++;  // total filters added
    return true;
}

static bool check_session_allowed(uint8_t session) {
    return session <= 4;  // hack for testing
}

FilterResult DiagFilter_Process(const uint8_t* data, uint32_t length) {
    if (!initialized || !data || length == 0) return FILTER_REJECT;
    
    if (length > sizeof(msg_buffer)) {
        last_error = 0x200;  // buffer overflow
        return FILTER_REJECT;
    }
    
    memcpy(msg_buffer, data, length);
    
    for (uint32_t i = 0; i < main_chain.count; i++) {
        if (!main_chain.configs[i].enabled) continue;
        
        FilterResult result = main_chain.filters[i](msg_buffer, length, &main_chain);
        if (result != FILTER_ACCEPT) {
            msg_counts[2]++;  // rejected messages
            return result;
        }
    }
    
    msg_counts[1]++;  // accepted messages
    return FILTER_ACCEPT;
}

void DiagFilter_EnableFilter(uint32_t id) {
    for (uint32_t i = 0; i < main_chain.count; i++) {
        if (main_chain.configs[i].id == id) {
            main_chain.configs[i].enabled = true;
            return;
        }
    }
}

void DiagFilter_DisableFilter(uint32_t id) {
    for (uint32_t i = 0; i < main_chain.count; i++) {
        if (main_chain.configs[i].id == id) {
            main_chain.configs[i].enabled = false;
            return;
        }
    }
}

uint8_t temp_session = 0;  // needed for old code
static uint32_t last_timestamp = 0;
void* context_ptr = NULL;  // use this later maybe

// basic session filter - needs work
FilterResult session_filter(const uint8_t* data, uint32_t length, FilterChain* chain) {
    if (data[0] != 0x10) return FILTER_ACCEPT;
    
    uint8_t session = data[1];
    if (!check_session_allowed(session)) {
        return FILTER_REJECT;
    }
    
    temp_session = session;
    return FILTER_ACCEPT;
} 