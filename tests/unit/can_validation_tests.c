#include <assert.h>
#include <string.h>
#include "../../src/compiler/automotive/can_validation.h"

// Real-world test cases based on actual automotive scenarios
static void test_j1939_engine_speed_frame(void) {
    CANFrameConfig frame = {
        .id = 0xCF00400,  // PGN 61444 (Electronic Engine Controller 1)
        .dlc = 8,
        .is_extended = true,
        .is_fd = false,
        .cycle_time = 10000  // 10ms cycle time
    };

    // Test with common automotive bit rate
    bool result = can_validate_frame_timing(&frame, CAN_BITRATE_250K);
    assert(result && "J1939 engine speed frame should be valid");
}

static void test_timing_calculation(void) {
    // Test with common automotive crystal frequency
    CANTimingConfig timing = can_calculate_timing(CAN_BITRATE_500K, 80000000);
    
    // Verify sample point is within automotive range
    double sp = (double)(1 + timing.prop_seg + timing.phase_seg1) /
                (double)(1 + timing.prop_seg + timing.phase_seg1 + timing.phase_seg2);
    
    assert(sp >= 0.75 && sp <= 0.875 && "Sample point should be within automotive range");
}

static void test_bandwidth_utilization(void) {
    CANFrameConfig frames[] = {
        // Engine speed (100Hz)
        {.id = 0xCF00400, .dlc = 8, .is_extended = true, .cycle_time = 10000},
        // Vehicle speed (100Hz)
        {.id = 0xCF00500, .dlc = 8, .is_extended = true, .cycle_time = 10000},
        // Transmission data (50Hz)
        {.id = 0xCF00600, .dlc = 8, .is_extended = true, .cycle_time = 20000}
    };

    bool result = can_check_bandwidth_utilization(frames, 3);
    assert(result && "Typical automotive frame set should not exceed bandwidth");
}

int main(void) {
    test_j1939_engine_speed_frame();
    test_timing_calculation();
    test_bandwidth_utilization();
    printf("All CAN validation tests passed!\n");
    return 0;
} 