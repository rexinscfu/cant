#ifndef CANT_J1939_H
#define CANT_J1939_H

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/can_driver.h"

// J1939 PGN definitions
typedef enum {
    J1939_PGN_ADDRESS_CLAIMED = 0x00EE00,
    J1939_PGN_REQUEST = 0x00EA00,
    J1939_PGN_DM1 = 0x00FECA,
    J1939_PGN_DM2 = 0x00FECB,
    J1939_PGN_DM3 = 0x00FECC,
    J1939_PGN_ELECTRONIC_ENGINE = 0x00F004,
    J1939_PGN_VEHICLE_SPEED = 0x00FEF1
} J1939PGN;

// J1939 configuration
typedef struct {
    uint8_t address;
    uint8_t name[8];
    uint16_t manufacturer_code;
    bool support_address_claim;
    uint32_t request_timeout_ms;
} J1939Config;

// J1939 message structure
typedef struct {
    uint32_t pgn;
    uint8_t priority;
    uint8_t source_address;
    uint8_t destination_address;
    uint8_t data[1785];  // Maximum J1939 payload
    size_t length;
} J1939Message;

typedef struct J1939Handler J1939Handler;

// Protocol API
J1939Handler* j1939_create(CANDriver* can_driver, const J1939Config* config);
void j1939_destroy(J1939Handler* handler);
bool j1939_transmit(J1939Handler* handler, const J1939Message* message);
bool j1939_receive(J1939Handler* handler, J1939Message* message, uint32_t timeout_ms);
void j1939_process(J1939Handler* handler);

// Advanced features
bool j1939_claim_address(J1939Handler* handler);
bool j1939_request_pgn(J1939Handler* handler, uint32_t pgn, uint8_t destination);
void j1939_set_address_changed_callback(J1939Handler* handler, void (*callback)(uint8_t));

#endif // CANT_J1939_H 