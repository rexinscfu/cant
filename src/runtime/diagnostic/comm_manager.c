#include "comm_manager.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

#define MAX_CHANNELS 8
#define MAX_MESSAGE_SIZE 4096

// Error Codes
#define COMM_ERROR_NONE 0
#define COMM_ERROR_TIMEOUT 1
#define COMM_ERROR_BUFFER_OVERFLOW 2
#define COMM_ERROR_INVALID_STATE 3
#define COMM_ERROR_TRANSMISSION_FAILED 4
#define COMM_ERROR_RECEPTION_FAILED 5

// Channel State
typedef struct {
    bool enabled;
    bool network_active;
    CommControlType control_state;
    Timer timeout_timer;
    uint32_t last_error;
    uint8_t rx_buffer[MAX_MESSAGE_SIZE];
    uint16_t rx_buffer_index;
    bool reception_in_progress;
} ChannelState;

// Internal state structure
typedef struct {
    CommManagerConfig config;
    CommChannelConfig channels[MAX_CHANNELS];
    ChannelState channel_states[MAX_CHANNELS];
    uint32_t channel_count;
    bool initialized;
    CriticalSection critical;
} CommManager;

static CommManager comm_manager;

static CommChannelConfig* find_channel(uint32_t channel_id) {
    for (uint32_t i = 0; i < comm_manager.channel_count; i++) {
        if (comm_manager.channels[i].channel_id == channel_id) {
            return &comm_manager.channels[i];
        }
    }
    return NULL;
}

static ChannelState* find_channel_state(uint32_t channel_id) {
    CommChannelConfig* channel = find_channel(channel_id);
    if (!channel) return NULL;
    
    uint32_t index = channel - comm_manager.channels;
    return &comm_manager.channel_states[index];
}

bool Comm_Manager_Init(const CommManagerConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    memcpy(&comm_manager.config, config, sizeof(CommManagerConfig));
    
    // Copy initial channels if provided
    if (config->channels && config->channel_count > 0) {
        uint32_t copy_count = (config->channel_count <= MAX_CHANNELS) ? 
                             config->channel_count : MAX_CHANNELS;
        memcpy(comm_manager.channels, config->channels, 
               sizeof(CommChannelConfig) * copy_count);
        
        // Initialize channel states
        for (uint32_t i = 0; i < copy_count; i++) {
            comm_manager.channel_states[i].enabled = true;
            comm_manager.channel_states[i].network_active = false;
            comm_manager.channel_states[i].control_state = COMM_CONTROL_ENABLE_RX_TX;
            comm_manager.channel_states[i].last_error = COMM_ERROR_NONE;
            comm_manager.channel_states[i].rx_buffer_index = 0;
            comm_manager.channel_states[i].reception_in_progress = false;
            timer_init();
        }
        
        comm_manager.channel_count = copy_count;
    } else {
        comm_manager.channel_count = 0;
    }

    init_critical(&comm_manager.critical);
    comm_manager.initialized = true;

    exit_critical(&comm_manager.critical);
    return true;
}

void Comm_Manager_DeInit(void) {
    enter_critical(&comm_manager.critical);
    
    // Disable all channels
    for (uint32_t i = 0; i < comm_manager.channel_count; i++) {
        comm_manager.channel_states[i].enabled = false;
        comm_manager.channel_states[i].network_active = false;
    }
    
    memset(&comm_manager, 0, sizeof(CommManager));
    exit_critical(&comm_manager.critical);
}

bool Comm_Manager_TransmitMessage(uint32_t channel_id, const uint8_t* data, uint16_t length) {
    if (!comm_manager.initialized || !data || length == 0) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    CommChannelConfig* channel = find_channel(channel_id);
    ChannelState* state = find_channel_state(channel_id);

    if (!channel || !state) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Check channel state
    if (!state->enabled || 
        !state->network_active || 
        state->control_state == COMM_CONTROL_DISABLE_RX_TX ||
        state->control_state == COMM_CONTROL_ENABLE_RX_DISABLE_TX) {
        state->last_error = COMM_ERROR_INVALID_STATE;
        exit_critical(&comm_manager.critical);
        return false;
    }

    bool result = false;
    if (channel->transmit) {
        result = channel->transmit(data, length);
        if (!result) {
            state->last_error = COMM_ERROR_TRANSMISSION_FAILED;
        }
    }

    exit_critical(&comm_manager.critical);
    return result;
}

void Comm_Manager_ProcessReceived(uint32_t channel_id, const uint8_t* data, uint16_t length) {
    if (!comm_manager.initialized || !data || length == 0) {
        return;
    }

    enter_critical(&comm_manager.critical);

    CommChannelConfig* channel = find_channel(channel_id);
    ChannelState* state = find_channel_state(channel_id);

    if (!channel || !state) {
        exit_critical(&comm_manager.critical);
        return;
    }

    // Check channel state
    if (!state->enabled || 
        !state->network_active || 
        state->control_state == COMM_CONTROL_DISABLE_RX_TX ||
        state->control_state == COMM_CONTROL_DISABLE_RX_ENABLE_TX) {
        state->last_error = COMM_ERROR_INVALID_STATE;
        exit_critical(&comm_manager.critical);
        return;
    }

    // Process received data
    if (state->reception_in_progress) {
        // Check for buffer overflow
        if ((state->rx_buffer_index + length) > MAX_MESSAGE_SIZE) {
            state->last_error = COMM_ERROR_BUFFER_OVERFLOW;
            state->reception_in_progress = false;
            state->rx_buffer_index = 0;
            exit_critical(&comm_manager.critical);
            return;
        }

        // Append data to buffer
        memcpy(&state->rx_buffer[state->rx_buffer_index], data, length);
        state->rx_buffer_index += length;

        // Reset timeout timer
        timer_start(&state->timeout_timer, channel->timeout_ms);
    } else {
        // Start new reception
        if (length <= MAX_MESSAGE_SIZE) {
            memcpy(state->rx_buffer, data, length);
            state->rx_buffer_index = length;
            state->reception_in_progress = true;
            timer_start(&state->timeout_timer, channel->timeout_ms);
        }
    }

    // Notify reception callback if configured
    if (channel->receive_callback) {
        channel->receive_callback(state->rx_buffer, state->rx_buffer_index);
    }

    exit_critical(&comm_manager.critical);
}

bool Comm_Manager_ControlCommunication(uint32_t channel_id, CommControlType control_type) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    ChannelState* state = find_channel_state(channel_id);
    if (!state) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    CommControlType old_state = state->control_state;
    state->control_state = control_type;

    if (comm_manager.config.state_change_callback) {
        comm_manager.config.state_change_callback(channel_id, control_type);
    }

    exit_critical(&comm_manager.critical);
    return true;
}

bool Comm_Manager_AddChannel(const CommChannelConfig* channel) {
    if (!comm_manager.initialized || !channel) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    // Check if channel already exists
    if (find_channel(channel->channel_id)) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Check if we have space for new channel
    if (comm_manager.channel_count >= MAX_CHANNELS) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Add new channel
    memcpy(&comm_manager.channels[comm_manager.channel_count], 
           channel, sizeof(CommChannelConfig));
    
    // Initialize channel state
    comm_manager.channel_states[comm_manager.channel_count].enabled = true;
    comm_manager.channel_states[comm_manager.channel_count].network_active = false;
    comm_manager.channel_states[comm_manager.channel_count].control_state = COMM_CONTROL_ENABLE_RX_TX;
    comm_manager.channel_states[comm_manager.channel_count].last_error = COMM_ERROR_NONE;
    comm_manager.channel_states[comm_manager.channel_count].rx_buffer_index = 0;
    comm_manager.channel_states[comm_manager.channel_count].reception_in_progress = false;
    timer_init();

    comm_manager.channel_count++;

    exit_critical(&comm_manager.critical);
    return true;
}

bool Comm_Manager_RemoveChannel(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    // Find channel index
    int32_t index = -1;
    for (uint32_t i = 0; i < comm_manager.channel_count; i++) {
        if (comm_manager.channels[i].channel_id == channel_id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Remove channel by shifting remaining ones
    if (index < (comm_manager.channel_count - 1)) {
        memmove(&comm_manager.channels[index],
                &comm_manager.channels[index + 1],
                sizeof(CommChannelConfig) * (comm_manager.channel_count - index - 1));
        memmove(&comm_manager.channel_states[index],
                &comm_manager.channel_states[index + 1],
                sizeof(ChannelState) * (comm_manager.channel_count - index - 1));
    }

    comm_manager.channel_count--;

    exit_critical(&comm_manager.critical);
    return true;
}

CommChannelConfig* Comm_Manager_GetChannel(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return NULL;
    }

    enter_critical(&comm_manager.critical);
    CommChannelConfig* channel = find_channel(channel_id);
    exit_critical(&comm_manager.critical);

    return channel;
}

bool Comm_Manager_IsChannelEnabled(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);
    
    ChannelState* state = find_channel_state(channel_id);
    bool enabled = state ? state->enabled : false;
    
    exit_critical(&comm_manager.critical);
    return enabled;
}

CommControlType Comm_Manager_GetChannelState(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return COMM_CONTROL_DISABLE_RX_TX;
    }

    enter_critical(&comm_manager.critical);
    
    ChannelState* state = find_channel_state(channel_id);
    CommControlType control_state = state ? state->control_state : COMM_CONTROL_DISABLE_RX_TX;
    
    exit_critical(&comm_manager.critical);
    return control_state;
}

void Comm_Manager_ProcessTimeout(void) {
    if (!comm_manager.initialized) {
        return;
    }

    enter_critical(&comm_manager.critical);

    for (uint32_t i = 0; i < comm_manager.channel_count; i++) {
        ChannelState* state = &comm_manager.channel_states[i];
        CommChannelConfig* channel = &comm_manager.channels[i];

        if (state->reception_in_progress && timer_expired(&state->timeout_timer)) {
            // Reception timeout occurred
            state->last_error = COMM_ERROR_TIMEOUT;
            state->reception_in_progress = false;
            state->rx_buffer_index = 0;

            if (comm_manager.config.error_callback) {
                comm_manager.config.error_callback(channel->channel_id, COMM_ERROR_TIMEOUT);
            }
        }
    }

    exit_critical(&comm_manager.critical);
}

uint32_t Comm_Manager_GetActiveChannels(void) {
    if (!comm_manager.initialized) {
        return 0;
    }

    uint32_t active_count = 0;
    
    enter_critical(&comm_manager.critical);
    
    for (uint32_t i = 0; i < comm_manager.channel_count; i++) {
        if (comm_manager.channel_states[i].enabled && 
            comm_manager.channel_states[i].network_active) {
            active_count++;
        }
    }
    
    exit_critical(&comm_manager.critical);
    return active_count;
}

bool Comm_Manager_ResetChannel(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    ChannelState* state = find_channel_state(channel_id);
    if (!state) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Reset channel state
    state->enabled = true;
    state->network_active = false;
    state->control_state = COMM_CONTROL_ENABLE_RX_TX;
    state->last_error = COMM_ERROR_NONE;
    state->rx_buffer_index = 0;
    state->reception_in_progress = false;
    timer_init();

    exit_critical(&comm_manager.critical);
    return true;
}

uint32_t Comm_Manager_GetLastError(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return COMM_ERROR_INVALID_STATE;
    }

    enter_critical(&comm_manager.critical);
    
    ChannelState* state = find_channel_state(channel_id);
    uint32_t error = state ? state->last_error : COMM_ERROR_INVALID_STATE;
    
    exit_critical(&comm_manager.critical);
    return error;
}

bool Comm_Manager_WakeupNetwork(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    ChannelState* state = find_channel_state(channel_id);
    if (!state || !state->enabled) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Implement network-specific wakeup procedure here
    // This is just a basic implementation
    state->network_active = true;
    state->control_state = COMM_CONTROL_ENABLE_RX_TX;
    state->last_error = COMM_ERROR_NONE;

    exit_critical(&comm_manager.critical);
    return true;
}

bool Comm_Manager_SleepNetwork(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);

    ChannelState* state = find_channel_state(channel_id);
    if (!state || !state->enabled) {
        exit_critical(&comm_manager.critical);
        return false;
    }

    // Implement network-specific sleep procedure here
    // This is just a basic implementation
    state->network_active = false;
    state->control_state = COMM_CONTROL_DISABLE_RX_TX;
    state->reception_in_progress = false;
    state->rx_buffer_index = 0;

    exit_critical(&comm_manager.critical);
    return true;
}

bool Comm_Manager_IsNetworkActive(uint32_t channel_id) {
    if (!comm_manager.initialized) {
        return false;
    }

    enter_critical(&comm_manager.critical);
    
    ChannelState* state = find_channel_state(channel_id);
    bool is_active = state ? state->network_active : false;
    
    exit_critical(&comm_manager.critical);
    return is_active;
}

