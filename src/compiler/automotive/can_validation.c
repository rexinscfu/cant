#include "can_validation.h"
#include <math.h>
#include <string.h>

// Real-world constants based on automotive standards
#define MAX_BUS_LOAD 0.8  // 80% maximum bus load recommended
#define MIN_SAMPLE_POINT 0.75  // 75% typical for automotive
#define MAX_SAMPLE_POINT 0.875 // 87.5% maximum recommended

static bool validate_j1939_pgn(uint32_t pgn) {
    // Reserved PGNs in J1939
    static const uint32_t reserved_pgns[] = {
        0xFED8, // Dedicated to OBD
        0xFED9, // Dedicated to OBD
        0xFEDA, // Reserved
        0xFEDB, // Reserved
        0x0000  // End marker
    };

    // Check against reserved PGNs
    for (size_t i = 0; reserved_pgns[i] != 0x0000; i++) {
        if (pgn == reserved_pgns[i]) {
            return false;
        }
    }

    // Validate PGN range
    return (pgn <= 0x3FFFF);  // Valid J1939 PGN range
}

bool can_validate_frame_timing(const CANFrameConfig* frame, CANBitRate bitrate) {
    if (!frame) return false;

    // Calculate frame size (including stuff bits, approximate)
    uint32_t base_bits = frame->is_extended ? 54 : 34;
    uint32_t data_bits = frame->dlc * 8;
    uint32_t total_bits = base_bits + data_bits;
    
    // Add 10% for stuff bits (worst case approximation)
    total_bits = (uint32_t)(total_bits * 1.1);

    // Calculate frame transmission time in microseconds
    uint32_t frame_time = (total_bits * 1000000) / bitrate;

    // Validate against cycle time
    if (frame_time >= frame->cycle_time) {
        return false;
    }

    // Check minimum cycle time (1ms for standard CAN)
    if (frame->cycle_time < 1000 && !frame->is_fd) {
        return false;
    }

    return true;
}

CANTimingConfig can_calculate_timing(CANBitRate bitrate, uint32_t clock_hz) {
    CANTimingConfig config = {0};
    
    // Target 80% sample point for automotive applications
    double target_sp = 0.8;
    
    // Calculate total time quantum
    uint32_t total_tq = clock_hz / bitrate;
    
    // Find suitable BRP
    for (uint16_t brp = 1; brp <= 1024; brp++) {
        uint32_t tq = total_tq / brp;
        
        // Valid TQ range for most controllers
        if (tq >= 8 && tq <= 25) {
            config.brp = brp;
            
            // Calculate segment lengths
            uint8_t total_seg = tq - 1;  // Sync_Seg is always 1
            uint8_t sp_tq = total_seg * target_sp;
            
            config.prop_seg = sp_tq / 2;
            config.phase_seg1 = sp_tq - config.prop_seg;
            config.phase_seg2 = total_seg - sp_tq;
            config.sjw = 1;  // Conservative default
            
            break;
        }
    }
    
    return config;
}

bool can_check_bandwidth_utilization(CANFrameConfig* frames, size_t frame_count) {
    if (!frames || frame_count == 0) return false;

    double total_utilization = 0.0;
    
    for (size_t i = 0; i < frame_count; i++) {
        CANFrameConfig* frame = &frames[i];
        
        // Calculate frame size including worst-case stuff bits
        uint32_t frame_bits = frame->is_extended ? 54 : 34;
        frame_bits += frame->dlc * 8;
        frame_bits = (uint32_t)(frame_bits * 1.1);  // Add 10% for stuff bits
        
        // Calculate utilization for this frame
        double frame_utilization = 
            (double)frame_bits / (double)frame->cycle_time;
        
        total_utilization += frame_utilization;
    }
    
    return total_utilization <= MAX_BUS_LOAD;
} 