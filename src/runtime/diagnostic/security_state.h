#ifndef CANT_SECURITY_STATE_H
#define CANT_SECURITY_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SECURITY_STATE_LOCKED,
    SECURITY_STATE_SEED_SENT,
    SECURITY_STATE_UNLOCKED,
    SECURITY_STATE_DELAY_ACTIVE
} SecurityState;

typedef struct {
    uint8_t level;
    SecurityState state;
    uint32_t timestamp;
    uint32_t seed;
    uint32_t attempts;
    uint32_t delay_end;
} SecurityStateContext;

bool Security_State_Init(SecurityStateContext* ctx, uint8_t level);
bool Security_State_RequestSeed(SecurityStateContext* ctx, uint32_t* seed);
bool Security_State_ValidateKey(SecurityStateContext* ctx, uint32_t key);
bool Security_State_Lock(SecurityStateContext* ctx);
bool Security_State_ProcessTimeout(SecurityStateContext* ctx);
SecurityState Security_State_GetState(const SecurityStateContext* ctx);

#endif // CANT_SECURITY_STATE_H 