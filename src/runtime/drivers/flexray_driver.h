#ifndef CANT_FLEXRAY_DRIVER_H
#define CANT_FLEXRAY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/queue.h"

// FlexRay frame structure
typedef struct {
    uint16_t slot_id;
    uint8_t cycle;
    uint8_t payload_length;
    bool is_startup;
    bool is_sync;
    bool is_null;
    uint8_t data[254];  // Maximum FlexRay payload
    uint64_t timestamp;
} FlexRayFrame;

// FlexRay configuration
typedef struct {
    uint32_t base_address;
    uint32_t interrupt_num;
    uint32_t gdcycle;       // Macro tick duration
    uint16_t pdynamic;      // Dynamic segment duration
    uint16_t pstatic;       // Static segment duration
    uint8_t static_slots;
    uint8_t dynamic_slots;
    bool dual_channel;
    struct {
        uint32_t baudrate;
        uint8_t sample_point;
        uint8_t sync_nodes;
    } timing;
} FlexRayConfig;

// FlexRay states
typedef enum {
    FLEXRAY_STATE_UNINIT,
    FLEXRAY_STATE_READY,
    FLEXRAY_STATE_WAKEUP,
    FLEXRAY_STATE_STARTUP,
    FLEXRAY_STATE_ACTIVE,
    FLEXRAY_STATE_PASSIVE,
    FLEXRAY_STATE_HALT
} FlexRayState;

// FlexRay statistics
typedef struct {
    uint32_t tx_frames;
    uint32_t rx_frames;
    uint32_t sync_frames;
    uint32_t null_frames;
    uint32_t syntax_errors;
    uint32_t content_errors;
    uint32_t slot_errors;
    struct {
        uint32_t channel_a;
        uint32_t channel_b;
    } communication_errors;
} FlexRayStats;

typedef struct FlexRayDriver FlexRayDriver;

// Driver API
FlexRayDriver* flexray_create(const FlexRayConfig* config);
void flexray_destroy(FlexRayDriver* driver);
bool flexray_start(FlexRayDriver* driver);
void flexray_stop(FlexRayDriver* driver);
bool flexray_transmit(FlexRayDriver* driver, const FlexRayFrame* frame);
bool flexray_receive(FlexRayDriver* driver, FlexRayFrame* frame, uint32_t timeout_ms);
FlexRayState flexray_get_state(const FlexRayDriver* driver);
void flexray_get_statistics(const FlexRayDriver* driver, FlexRayStats* stats);

// Advanced features
bool flexray_configure_slot(FlexRayDriver* driver, uint16_t slot_id, bool is_transmit);
bool flexray_set_cycle_trigger(FlexRayDriver* driver, uint8_t cycle, void (*callback)(void*), void* arg);
bool flexray_sync_status(const FlexRayDriver* driver);

#endif // CANT_FLEXRAY_DRIVER_H 