#include "target_info.h"
#include <string.h>

// Target information
static const char* TARGET_NAME = "S32K3";
static const char* TARGET_DESCRIPTION = "NXP S32K3 Automotive MCU";

// Target-specific SIMD support
static const uint32_t S32K3_SIMD_WIDTH = 16;  // 128-bit SIMD
static const uint32_t S32K3_SIMD_ALIGN = 16;  // 16-byte alignment

// Enhanced target capabilities
static const S32K3Capabilities S32K3_CAPABILITIES = {
    .max_can_filters = 128,
    .max_frame_size = 64,
    .max_baudrate = 8000000,  // 8 Mbps for CAN FD
    .min_baudrate = 125000,   // 125 kbps
    .has_fd_support = true,
    .has_auto_retransmit = true,
    .simd_width = S32K3_SIMD_WIDTH,
    .simd_align = S32K3_SIMD_ALIGN,
    .supports_hardware_filtering = true,
    .max_pattern_length = 64
};

// Target-specific intrinsics for SIMD operations
static const S32K3IntrinsicInfo SIMD_INTRINSICS[] = {
    {
        .name = "s32k3_simd_load",
        .num_args = 2,
        .is_pure = true,
        .has_side_effects = false
    },
    {
        .name = "s32k3_simd_store",
        .num_args = 2,
        .is_pure = false,
        .has_side_effects = true
    },
    {
        .name = "s32k3_simd_compare",
        .num_args = 3,
        .is_pure = true,
        .has_side_effects = false
    },
    {
        .name = "s32k3_simd_mask",
        .num_args = 2,
        .is_pure = true,
        .has_side_effects = false
    }
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
    return S32K3_CAPABILITIES;
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
    if (config->can_frequency < S32K3_CAPABILITIES.min_baudrate ||
        config->can_frequency > S32K3_CAPABILITIES.max_baudrate) {
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

// Get SIMD capabilities
bool s32k3_get_simd_info(uint32_t* width, uint32_t* alignment) {
    if (!width || !alignment) return false;
    
    if (s32k3_has_feature(S32K3_FEATURE_SIMD)) {
        *width = S32K3_SIMD_WIDTH;
        *alignment = S32K3_SIMD_ALIGN;
        return true;
    }
    
    return false;
}

// Check if pattern can use hardware acceleration
bool s32k3_can_accelerate_pattern(const uint8_t* pattern, uint32_t length) {
    if (!pattern || length > S32K3_CAPABILITIES.max_pattern_length) {
        return false;
    }
    
    // Check alignment requirements
    if ((uintptr_t)pattern % S32K3_SIMD_ALIGN != 0) {
        return false;
    }
    
    // Check pattern length is multiple of SIMD width
    if (length % S32K3_SIMD_WIDTH != 0) {
        return false;
    }
    
    return true;
} 