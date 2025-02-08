#include "target_info.h"
#include <string.h>

// Target information
static const char* TARGET_NAME = "S32K3";
static const char* TARGET_DESCRIPTION = "NXP S32K3 Automotive MCU";

// Default capabilities
static const S32K3Capabilities DEFAULT_CAPABILITIES = {
    .max_can_filters = 128,
    .max_frame_size = 64,
    .max_baudrate = 8000000,  // 8 Mbps for CAN FD
    .min_baudrate = 125000,   // 125 kbps
    .has_fd_support = true,
    .has_auto_retransmit = true
};

// Intrinsic definitions
static const S32K3IntrinsicInfo INTRINSICS[] = {
    [S32K3_INTRINSIC_CAN_SEND] = {
        .name = "s32k3_can_send",
        .num_args = 3,
        .is_pure = false,
        .has_side_effects = true
    },
    [S32K3_INTRINSIC_CAN_RECEIVE] = {
        .name = "s32k3_can_receive",
        .num_args = 2,
        .is_pure = false,
        .has_side_effects = true
    },
    [S32K3_INTRINSIC_PATTERN_MATCH] = {
        .name = "s32k3_pattern_match",
        .num_args = 4,
        .is_pure = true,
        .has_side_effects = false
    },
    [S32K3_INTRINSIC_DMA_TRANSFER] = {
        .name = "s32k3_dma_transfer",
        .num_args = 4,
        .is_pure = false,
        .has_side_effects = true
    },
    [S32K3_INTRINSIC_SIMD_COMPARE] = {
        .name = "s32k3_simd_compare",
        .num_args = 3,
        .is_pure = true,
        .has_side_effects = false
    }
};

// Implementation of target information API
const char* s32k3_get_target_name(void) {
    return TARGET_NAME;
}

const char* s32k3_get_target_description(void) {
    return TARGET_DESCRIPTION;
}

S32K3Capabilities s32k3_get_capabilities(void) {
    return DEFAULT_CAPABILITIES;
}

bool s32k3_has_feature(S32K3Feature feature) {
    // Check if feature is supported in current configuration
    switch (feature) {
        case S32K3_FEATURE_SIMD:
            #ifdef S32K3_HAS_SIMD
            return true;
            #else
            return false;
            #endif
            
        case S32K3_FEATURE_FPU:
            #ifdef S32K3_HAS_FPU
            return true;
            #else
            return false;
            #endif
            
        case S32K3_FEATURE_DSP:
            #ifdef S32K3_HAS_DSP
            return true;
            #else
            return false;
            #endif
            
        case S32K3_FEATURE_SECURITY:
            #ifdef S32K3_HAS_SECURITY
            return true;
            #else
            return false;
            #endif
            
        case S32K3_FEATURE_CAN_FD:
            #ifdef S32K3_HAS_CAN_FD
            return true;
            #else
            return false;
            #endif
            
        default:
            return false;
    }
}

bool s32k3_validate_config(const S32K3Config* config) {
    if (!config) return false;
    
    // Validate CPU frequency
    if (config->cpu_frequency < 8000000 || config->cpu_frequency > 160000000) {
        return false;
    }
    
    // Validate CAN frequency
    if (config->can_frequency < DEFAULT_CAPABILITIES.min_baudrate ||
        config->can_frequency > DEFAULT_CAPABILITIES.max_baudrate) {
        return false;
    }
    
    // Validate memory sizes
    if (config->ram_size < 64 * 1024 || config->ram_size > 1024 * 1024) {
        return false;
    }
    
    if (config->flash_size < 512 * 1024 || config->flash_size > 4 * 1024 * 1024) {
        return false;
    }
    
    // Validate features
    uint32_t valid_features = S32K3_FEATURE_SIMD | S32K3_FEATURE_FPU |
                             S32K3_FEATURE_DSP | S32K3_FEATURE_SECURITY |
                             S32K3_FEATURE_CAN_FD;
                             
    if (config->features & ~valid_features) {
        return false;
    }
    
    return true;
}

const S32K3IntrinsicInfo* s32k3_get_intrinsic_info(S32K3Intrinsic intrinsic) {
    if (intrinsic >= sizeof(INTRINSICS) / sizeof(INTRINSICS[0])) {
        return NULL;
    }
    return &INTRINSICS[intrinsic];
}

bool s32k3_can_inline_intrinsic(S32K3Intrinsic intrinsic) {
    const S32K3IntrinsicInfo* info = s32k3_get_intrinsic_info(intrinsic);
    if (!info) return false;
    
    // Only pure functions with no side effects can be inlined
    return info->is_pure && !info->has_side_effects;
} 