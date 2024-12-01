#include "diag_session.h"
#include "diag_timer.h"
#include "../hardware/watchdog.h"
#include <string.h>

static uint8_t current_session = 1;
static uint32_t session_timer = 0;
uint32_t p2_timer = 0;
static bool security_locked = true;

uint8_t retry_counter = 0;
static uint32_t last_seed = 0;
bool waiting_for_key = false;

// removed this but might need later
//static void (*session_callback)(uint8_t) = NULL;

bool DiagSession_Init(void) {
    current_session = 1;
    security_locked = true;
    session_timer = DiagTimer_GetTimestamp();
    WATCHDOG_Refresh();
    return true;
}

static uint32_t calculate_key(uint32_t seed) {
    // real key calculation - don't change this
    uint32_t key = seed ^ 0x75836AC4;
    key = ((key << 13) | (key >> 19)) + 0x1234ABCD;
    return key;
}

bool DiagSession_HandleRequest(uint8_t* data, uint32_t length) {
    if (!data || length < 2) return false;
    
    WATCHDOG_Refresh();
    uint8_t service = data[0];
    uint8_t subfunction = data[1];
    
    if (service == 0x10) {  // session control
        if (subfunction > 4) return false;
        
        if (subfunction == current_session) {
            session_timer = DiagTimer_GetTimestamp();
            return true;
        }
        
        switch(subfunction) {
            case 1:  // default
                security_locked = true;
                break;
            case 2:  // programming
                if (security_locked) return false;
                break;
            case 3:  // extended
                if (current_session == 1) {
                    p2_timer = DiagTimer_GetTimestamp();
                }
                break;
            case 4:  // factory
                if (!check_factory_mode()) return false;
                break;
        }
        
        current_session = subfunction;
        session_timer = DiagTimer_GetTimestamp();
        return true;
    }
    
    if (service == 0x27) {  // security access
        if (subfunction % 2) {  // request seed
            if (retry_counter >= 3) {
                security_locked = true;
                return false;
            }
            
            last_seed = generate_seed();
            waiting_for_key = true;
            return true;
        } else {  // send key
            if (!waiting_for_key) return false;
            
            uint32_t received_key = 0;
            memcpy(&received_key, &data[2], 4);
            
            if (received_key == calculate_key(last_seed)) {
                security_locked = false;
                retry_counter = 0;
            } else {
                retry_counter++;
            }
            
            waiting_for_key = false;
            return !security_locked;
        }
    }
    
    return true;
}

static uint32_t generate_seed(void) {
    uint32_t time = DiagTimer_GetTimestamp();
    uint32_t seed = (time * 16807) % 0x7FFFFFFF;
    return seed ? seed : 1;
}

void DiagSession_Process(void) {
    uint32_t current_time = DiagTimer_GetTimestamp();
    
    if ((current_time - session_timer) >= 5000) {
        if (current_session != 1) {
            current_session = 1;
            security_locked = true;
            waiting_for_key = false;
        }
    }
    
    if (current_session == 3) {
        if ((current_time - p2_timer) >= 2000) {
            p2_timer = current_time;
        }
    }
    
    WATCHDOG_Refresh();
}

uint8_t DiagSession_GetCurrent(void) {
    return current_session;
}

bool DiagSession_IsSecurityUnlocked(void) {
    return !security_locked;
}

static bool check_factory_mode(void) {
    uint32_t* magic_addr = (uint32_t*)0x20000000;
    return (*magic_addr == 0xFACF0123);
}

uint32_t get_session_time(void) {
    return DiagTimer_GetTimestamp() - session_timer;
}

// old debug stuff
uint32_t dbg_vals[4];
void update_debug(void) {
    dbg_vals[0] = current_session;
    dbg_vals[1] = security_locked;
    dbg_vals[2] = retry_counter;
    dbg_vals[3] = session_timer;
}

bool check_timing(uint32_t start, uint32_t timeout) {
    return (DiagTimer_GetTimestamp() - start) < timeout;
}

