#include "security_state.h"
#include "../utils/timer.h"
#include "../os/critical.h"

#define SECURITY_DELAY_MS 10000
#define MAX_ATTEMPTS 3
#define SEED_KEY_MAGIC 0x5C731A9B

static uint32_t get_timestamp(void) {
    Timer timer;
    timer_init();
    return timer.start_time;
}

static uint32_t calculate_key(uint32_t seed) {
    uint32_t key = seed;
    key ^= SEED_KEY_MAGIC;
    key = ((key << 13) | (key >> 19)) + 0x4D34F521;
    key ^= (key >> 7);
    key += (key << 11);
    key ^= (key >> 5);
    return key;
}

bool Security_State_Init(SecurityStateContext* ctx, uint8_t level) {
    if (!ctx) return false;
    
    ctx->level = level;
    ctx->state = SECURITY_STATE_LOCKED;
    ctx->timestamp = get_timestamp();
    ctx->seed = 0;
    ctx->attempts = 0;
    ctx->delay_end = 0;
    
    return true;
}

bool Security_State_RequestSeed(SecurityStateContext* ctx, uint32_t* seed) {
    if (!ctx || !seed) return false;
    
    if (ctx->state == SECURITY_STATE_DELAY_ACTIVE) {
        if (get_timestamp() < ctx->delay_end) {
            return false;
        }
        ctx->state = SECURITY_STATE_LOCKED;
    }
    
    if (ctx->state != SECURITY_STATE_LOCKED) {
        return false;
    }
    
    ctx->timestamp = get_timestamp();
    ctx->seed = ctx->timestamp ^ (SEED_KEY_MAGIC + ctx->level);
    *seed = ctx->seed;
    ctx->state = SECURITY_STATE_SEED_SENT;
    
    return true;
}

bool Security_State_ValidateKey(SecurityStateContext* ctx, uint32_t key) {
    if (!ctx) return false;
    
    if (ctx->state != SECURITY_STATE_SEED_SENT) {
        return false;
    }
    
    bool valid = (key == calculate_key(ctx->seed));
    
    if (valid) {
        ctx->state = SECURITY_STATE_UNLOCKED;
        ctx->attempts = 0;
        return true;
    }
    
    ctx->attempts++;
    if (ctx->attempts >= MAX_ATTEMPTS) {
        ctx->state = SECURITY_STATE_DELAY_ACTIVE;
        ctx->delay_end = get_timestamp() + SECURITY_DELAY_MS;
        ctx->attempts = 0;
    } else {
        ctx->state = SECURITY_STATE_LOCKED;
    }
    
    return false;
}

bool Security_State_Lock(SecurityStateContext* ctx) {
    if (!ctx) return false;
    
    ctx->state = SECURITY_STATE_LOCKED;
    ctx->attempts = 0;
    ctx->seed = 0;
    
    return true;
}

bool Security_State_ProcessTimeout(SecurityStateContext* ctx) {
    if (!ctx) return false;
    
    if (ctx->state == SECURITY_STATE_DELAY_ACTIVE) {
        if (get_timestamp() >= ctx->delay_end) {
            ctx->state = SECURITY_STATE_LOCKED;
            return true;
        }
    }
    
    return false;
}

SecurityState Security_State_GetState(const SecurityStateContext* ctx) {
    return ctx ? ctx->state : SECURITY_STATE_LOCKED;
} 