#ifndef CANT_CAN_VALIDATION_H
#define CANT_CAN_VALIDATION_H

#include <stdint.h>
#include <stdbool.h>

// Common CAN bit rates used in automotive
typedef enum {
    CAN_BITRATE_125K = 125000,
    CAN_BITRATE_250K = 250000,
    CAN_BITRATE_500K = 500000,
    CAN_BITRATE_1M = 1000000
} CANBitRate;

typedef struct {
    uint32_t id;           // CAN ID
    uint8_t dlc;          // Data Length Code
    bool is_extended;     // Standard vs Extended frame
    bool is_fd;          // CAN FD support
    uint32_t cycle_time;  // in microseconds
} CANFrameConfig;

typedef struct {
    uint8_t prop_seg;
    uint8_t phase_seg1;
    uint8_t phase_seg2;
    uint8_t sjw;         // Sync Jump Width
    uint16_t brp;        // Bit Rate Prescaler
} CANTimingConfig;

// Practical validation functions
bool can_validate_frame_timing(const CANFrameConfig* frame, CANBitRate bitrate);
bool can_validate_pgn(uint32_t pgn);
CANTimingConfig can_calculate_timing(CANBitRate bitrate, uint32_t clock_hz);
bool can_check_bandwidth_utilization(CANFrameConfig* frames, size_t frame_count);

#endif // CANT_CAN_VALIDATION_H 