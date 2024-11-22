#include "flow_monitor.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

#define MAX_CALL_STACK_SIZE 32

// Internal state
static struct {
    FlowMonitorConfig config;
    struct {
        uint32_t current_checkpoint;
        uint32_t total_errors;
        uint32_t call_stack[MAX_CALL_STACK_SIZE];
        uint32_t stack_depth;
        Timer checkpoint_timer;
        bool initialized;
    } state;
    CriticalSection critical;
} flow_monitor;

// Internal helper functions
static bool validate_checkpoint_id(uint32_t checkpoint_id) {
    return checkpoint_id < flow_monitor.config.checkpoint_count;
}

static bool is_valid_transition(uint32_t from_id, uint32_t to_id) {
    const FlowCheckpointConfig* from = &flow_monitor.config.checkpoints[from_id];
    return (to_id == from->expected_next || to_id == from->alternate_next);
}

static void handle_error(FlowMonitorResult result, uint32_t checkpoint_id) {
    flow_monitor.state.total_errors++;
    
    if (flow_monitor.config.error_callback) {
        flow_monitor.config.error_callback(result, checkpoint_id);
    }
}

bool Flow_Monitor_Init(const FlowMonitorConfig* config) {
    if (!config || !config->checkpoints || config->checkpoint_count == 0) {
        return false;
    }
    
    enter_critical(&flow_monitor.critical);
    
    memset(&flow_monitor.state, 0, sizeof(flow_monitor.state));
    memcpy(&flow_monitor.config, config, sizeof(FlowMonitorConfig));
    
    timer_init();
    init_critical(&flow_monitor.critical);
    
    flow_monitor.state.initialized = true;
    
    exit_critical(&flow_monitor.critical);
    return true;
}

void Flow_Monitor_DeInit(void) {
    enter_critical(&flow_monitor.critical);
    flow_monitor.state.initialized = false;
    memset(&flow_monitor.state, 0, sizeof(flow_monitor.state));
    exit_critical(&flow_monitor.critical);
}

FlowMonitorResult Flow_Monitor_Checkpoint(uint32_t checkpoint_id) {
    if (!flow_monitor.state.initialized || 
        !validate_checkpoint_id(checkpoint_id)) {
        return FLOW_MONITOR_ERROR_CONFIG;
    }
    
    FlowMonitorResult result = FLOW_MONITOR_OK;
    
    enter_critical(&flow_monitor.critical);
    
    const FlowCheckpointConfig* checkpoint = 
        &flow_monitor.config.checkpoints[checkpoint_id];
    
    // Validate sequence
    if (flow_monitor.state.current_checkpoint != 0 && 
        !is_valid_transition(flow_monitor.state.current_checkpoint, checkpoint_id)) {
        result = FLOW_MONITOR_ERROR_SEQUENCE;
        handle_error(result, checkpoint_id);
        goto exit;
    }
    
    // Check timing constraints
    if (checkpoint->min_time_ms > 0 || checkpoint->max_time_ms > 0) {
        uint32_t elapsed = timer_remaining(&flow_monitor.state.checkpoint_timer);
        
        if (elapsed < checkpoint->min_time_ms || 
            elapsed > checkpoint->max_time_ms) {
            result = FLOW_MONITOR_ERROR_TIMING;
            handle_error(result, checkpoint_id);
            goto exit;
        }
    }
    
    // Handle call stack
    switch (checkpoint->type) {
        case FLOW_CHECKPOINT_CALL:
            if (flow_monitor.state.stack_depth >= flow_monitor.config.max_stack_depth) {
                result = FLOW_MONITOR_ERROR_STACK;
                handle_error(result, checkpoint_id);
                goto exit;
            }
            flow_monitor.state.call_stack[flow_monitor.state.stack_depth++] = 
                flow_monitor.state.current_checkpoint;
            break;
            
        case FLOW_CHECKPOINT_RETURN:
            if (flow_monitor.state.stack_depth == 0) {
                result = FLOW_MONITOR_ERROR_STACK;
                handle_error(result, checkpoint_id);
                goto exit;
            }
            flow_monitor.state.stack_depth--;
            break;
            
        default:
            break;
    }
    
    // Update state
    flow_monitor.state.current_checkpoint = checkpoint_id;
    timer_start(&flow_monitor.state.checkpoint_timer, checkpoint->max_time_ms);
    
exit:
    exit_critical(&flow_monitor.critical);
    return result;
}

bool Flow_Monitor_IsSequenceValid(void) {
    if (!flow_monitor.state.initialized) return false;
    
    bool valid;
    
    enter_critical(&flow_monitor.critical);
    valid = (flow_monitor.state.total_errors == 0);
    exit_critical(&flow_monitor.critical);
    
    return valid;
}

uint32_t Flow_Monitor_GetErrorCount(void) {
    return flow_monitor.state.total_errors;
}

void Flow_Monitor_ResetErrorCount(void) {
    enter_critical(&flow_monitor.critical);
    flow_monitor.state.total_errors = 0;
    exit_critical(&flow_monitor.critical);
}

void Flow_Monitor_GetStatus(uint32_t* current_checkpoint, uint32_t* error_count) {
    if (!current_checkpoint || !error_count) return;
    
    enter_critical(&flow_monitor.critical);
    *current_checkpoint = flow_monitor.state.current_checkpoint;
    *error_count = flow_monitor.state.total_errors;
    exit_critical(&flow_monitor.critical);
}

bool Flow_Monitor_ValidateFlow(uint32_t start_checkpoint, uint32_t end_checkpoint) {
    if (!flow_monitor.state.initialized || 
        !validate_checkpoint_id(start_checkpoint) || 
        !validate_checkpoint_id(end_checkpoint)) {
        return false;
    }
    
    bool valid = true;
    uint32_t current = start_checkpoint;
    uint32_t visited[256] = {0};  // Prevent infinite loops
    
    while (current != end_checkpoint) {
        if (visited[current]++ > 0) {
            valid = false;
            break;
        }
        
        const FlowCheckpointConfig* checkpoint = 
            &flow_monitor.config.checkpoints[current];
            
        if (!validate_checkpoint_id(checkpoint->expected_next) && 
            !validate_checkpoint_id(checkpoint->alternate_next)) {
            valid = false;
            break;
        }
        
        current = checkpoint->expected_next;
    }
    
    return valid;
} 