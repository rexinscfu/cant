#ifndef CANT_ISOTP_H
#define CANT_ISOTP_H

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/can_driver.h"

// ISO-TP frame types
typedef enum {
    ISOTP_SINGLE_FRAME = 0,
    ISOTP_FIRST_FRAME = 1,
    ISOTP_CONSECUTIVE_FRAME = 2,
    ISOTP_FLOW_CONTROL = 3
} ISOTPFrameType;

// ISO-TP configuration
typedef struct {
    uint32_t rx_id;
    uint32_t tx_id;
    bool extended_addressing;
    uint16_t stmin;        // Separation time minimum (ms)
    uint8_t blocksize;     // Flow control block size
    uint32_t timeout_ms;   // Response timeout
} ISOTPConfig;

// ISO-TP context
typedef struct ISOTP ISOTP;

// Protocol API
ISOTP* isotp_create(CANDriver* can_driver, const ISOTPConfig* config);
void isotp_destroy(ISOTP* isotp);
bool isotp_transmit(ISOTP* isotp, const uint8_t* data, size_t length);
bool isotp_receive(ISOTP* isotp, uint8_t* data, size_t* length, uint32_t timeout_ms);
void isotp_process(ISOTP* isotp);

#endif // CANT_ISOTP_H 