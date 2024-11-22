#ifndef CANT_CAN_DRIVER_H
#define CANT_CAN_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/queue.h"

// CAN frame structure aligned with AUTOSAR
typedef struct {
    uint32_t id;
    uint8_t dlc;
    bool is_extended;
    bool is_fd;
    bool is_remote;
    uint8_t data[64];  // Support for CAN-FD
    uint64_t timestamp;
} CANFrame;

// CAN controller configuration
typedef struct {
    uint32_t base_address;
    uint32_t interrupt_num;
    uint32_t bitrate;
    uint32_t sample_point;
    bool fd_enabled;
    bool auto_retransmit;
    uint8_t tx_mailboxes;
    uint8_t rx_mailboxes;
} CANConfig;

// CAN controller states
typedef enum {
    CAN_STATE_UNINIT,
    CAN_STATE_STOPPED,
    CAN_STATE_STARTED,
    CAN_STATE_SLEEP,
    CAN_STATE_ERROR_ACTIVE,
    CAN_STATE_ERROR_PASSIVE,
    CAN_STATE_BUS_OFF
} CANState;

// CAN error types
typedef enum {
    CAN_ERROR_NONE,
    CAN_ERROR_STUFF,
    CAN_ERROR_FORM,
    CAN_ERROR_ACK,
    CAN_ERROR_BIT1,
    CAN_ERROR_BIT0,
    CAN_ERROR_CRC,
    CAN_ERROR_SOFTWARE,
    CAN_ERROR_HARDWARE
} CANError;

// CAN statistics
typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    uint32_t overflow_count;
    uint32_t bus_off_count;
    uint8_t tx_error_counter;
    uint8_t rx_error_counter;
} CANStats;

// CAN driver handle
typedef struct CANDriver CANDriver;

// Driver API
CANDriver* can_create(const CANConfig* config);
void can_destroy(CANDriver* driver);
bool can_start(CANDriver* driver);
void can_stop(CANDriver* driver);
bool can_transmit(CANDriver* driver, const CANFrame* frame, uint32_t timeout_ms);
bool can_receive(CANDriver* driver, CANFrame* frame, uint32_t timeout_ms);
CANState can_get_state(const CANDriver* driver);
CANError can_get_last_error(const CANDriver* driver);
void can_get_statistics(const CANDriver* driver, CANStats* stats);
bool can_set_filter(CANDriver* driver, uint32_t id, uint32_t mask, bool is_extended);
void can_clear_filters(CANDriver* driver);

// Advanced features
bool can_enable_fd(CANDriver* driver);
bool can_set_bitrate(CANDriver* driver, uint32_t bitrate, uint32_t data_bitrate);
bool can_enter_sleep(CANDriver* driver);
bool can_exit_sleep(CANDriver* driver);
void can_reset(CANDriver* driver);

#endif // CANT_CAN_DRIVER_H 