#include "can_driver.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../utils/error.h"
#include "../os/critical.h"

// Internal structures
struct CANDriver {
    CANConfig config;
    CANState state;
    CANError last_error;
    CANStats statistics;
    
    // Hardware registers (platform specific)
    volatile uint32_t* can_base;
    
    // Message queues
    Queue tx_queue;
    Queue rx_queue;
    
    // Filter bank
    struct {
        uint32_t* ids;
        uint32_t* masks;
        bool* is_extended;
        size_t count;
        size_t capacity;
    } filters;
    
    // Interrupt handling
    struct {
        bool tx_complete;
        bool rx_pending;
        bool error_pending;
    } interrupts;
    
    // Critical section protection
    CriticalSection critical;
};

// Hardware register offsets (example for S32K3)
#define CAN_MCR     0x00
#define CAN_CTRL1   0x04
#define CAN_TIMER   0x08
#define CAN_RXMGMASK 0x10
#define CAN_ESR1    0x20
#define CAN_IMASK1  0x28
#define CAN_IFLAG1  0x30

// Helper functions
static bool configure_hardware(CANDriver* driver) {
    assert(driver != NULL);
    
    // Reset controller
    driver->can_base[CAN_MCR] = 0x5000000F;
    
    // Wait for reset to complete
    uint32_t timeout = 1000000;
    while ((driver->can_base[CAN_MCR] & 0x01000000) && --timeout > 0) {
        __asm volatile ("nop");
    }
    
    if (timeout == 0) {
        driver->last_error = CAN_ERROR_HARDWARE;
        return false;
    }
    
    // Configure bit timing
    uint32_t ctrl1 = 0;
    if (driver->config.fd_enabled) {
        ctrl1 |= 0x00800000;  // Enable FD mode
    }
    
    // Calculate timing parameters
    uint32_t clock = 80000000;  // Assuming 80MHz clock
    uint32_t tq = clock / driver->config.bitrate;
    
    // Set sample point
    uint32_t prop_seg = (tq * driver->config.sample_point) / 100;
    uint32_t phase_seg1 = prop_seg / 2;
    uint32_t phase_seg2 = tq - prop_seg - 1;
    
    ctrl1 |= ((phase_seg2 - 1) << 0)  |
             ((phase_seg1 - 1) << 8)  |
             ((prop_seg - 1) << 16);
             
    driver->can_base[CAN_CTRL1] = ctrl1;
    
    // Configure mailboxes
    for (uint8_t i = 0; i < driver->config.tx_mailboxes; i++) {
        // Configure TX mailbox
    }
    
    for (uint8_t i = 0; i < driver->config.rx_mailboxes; i++) {
        // Configure RX mailbox
    }
    
    // Enable interrupts
    driver->can_base[CAN_IMASK1] = 0xFFFFFFFF;
    
    return true;
}

static void process_rx_interrupt(CANDriver* driver) {
    assert(driver != NULL);
    
    uint32_t flags = driver->can_base[CAN_IFLAG1];
    
    for (uint8_t i = 0; i < driver->config.rx_mailboxes; i++) {
        if (flags & (1 << i)) {
            // Read frame from mailbox
            CANFrame frame;
            // ... read frame data ...
            
            // Add to RX queue
            if (!queue_push(&driver->rx_queue, &frame, sizeof(frame))) {
                driver->statistics.overflow_count++;
            } else {
                driver->statistics.rx_count++;
            }
            
            // Clear interrupt flag
            driver->can_base[CAN_IFLAG1] = (1 << i);
        }
    }
}

static void process_tx_interrupt(CANDriver* driver) {
    assert(driver != NULL);
    
    uint32_t flags = driver->can_base[CAN_IFLAG1];
    
    for (uint8_t i = 0; i < driver->config.tx_mailboxes; i++) {
        if (flags & (1 << (i + 8))) {  // TX mailboxes start at bit 8
            driver->statistics.tx_count++;
            
            // Clear interrupt flag
            driver->can_base[CAN_IFLAG1] = (1 << (i + 8));
            
            // Check if more frames to send
            CANFrame frame;
            if (queue_pop(&driver->tx_queue, &frame, sizeof(frame))) {
                // Load next frame into mailbox
                // ... write frame data ...
            }
        }
    }
}

// Interrupt handler
void can_irq_handler(CANDriver* driver) {
    assert(driver != NULL);
    
    enter_critical(&driver->critical);
    
    uint32_t esr1 = driver->can_base[CAN_ESR1];
    
    // Check for errors
    if (esr1 & 0x00000002) {  // Bus off
        driver->state = CAN_STATE_BUS_OFF;
        driver->statistics.bus_off_count++;
    }
    
    // Process RX/TX interrupts
    process_rx_interrupt(driver);
    process_tx_interrupt(driver);
    
    exit_critical(&driver->critical);
}

// Driver API implementation
CANDriver* can_create(const CANConfig* config) {
    if (!config) return NULL;
    
    CANDriver* driver = calloc(1, sizeof(CANDriver));
    if (!driver) return NULL;
    
    // Copy configuration
    memcpy(&driver->config, config, sizeof(CANConfig));
    
    // Initialize queues
    if (!queue_init(&driver->tx_queue, sizeof(CANFrame), 32) ||
        !queue_init(&driver->rx_queue, sizeof(CANFrame), 32)) {
        free(driver);
        return NULL;
    }
    
    // Initialize filter bank
    driver->filters.capacity = 16;
    driver->filters.ids = calloc(driver->filters.capacity, sizeof(uint32_t));
    driver->filters.masks = calloc(driver->filters.capacity, sizeof(uint32_t));
    driver->filters.is_extended = calloc(driver->filters.capacity, sizeof(bool));
    
    if (!driver->filters.ids || !driver->filters.masks || !driver->filters.is_extended) {
        free(driver->filters.ids);
        free(driver->filters.masks);
        free(driver->filters.is_extended);
        queue_destroy(&driver->tx_queue);
        queue_destroy(&driver->rx_queue);
        free(driver);
        return NULL;
    }
    
    // Map hardware registers
    driver->can_base = (volatile uint32_t*)config->base_address;
    
    // Initialize critical section
    init_critical(&driver->critical);
    
    driver->state = CAN_STATE_STOPPED;
    return driver;
}

void can_destroy(CANDriver* driver) {
    if (!driver) return;
    
    can_stop(driver);
    
    free(driver->filters.ids);
    free(driver->filters.masks);
    free(driver->filters.is_extended);
    queue_destroy(&driver->tx_queue);
    queue_destroy(&driver->rx_queue);
    destroy_critical(&driver->critical);
    
    free(driver);
}

bool can_start(CANDriver* driver) {
    if (!driver || driver->state != CAN_STATE_STOPPED) return false;
    
    if (!configure_hardware(driver)) {
        return false;
    }
    
    driver->state = CAN_STATE_STARTED;
    return true;
}

void can_stop(CANDriver* driver) {
    if (!driver || driver->state == CAN_STATE_STOPPED) return;
    
    enter_critical(&driver->critical);
    
    // Disable controller
    driver->can_base[CAN_MCR] |= 0x80000000;
    
    // Clear queues
    queue_clear(&driver->tx_queue);
    queue_clear(&driver->rx_queue);
    
    driver->state = CAN_STATE_STOPPED;
    
    exit_critical(&driver->critical);
}

bool can_transmit(CANDriver* driver, const CANFrame* frame, uint32_t timeout_ms) {
    if (!driver || !frame || driver->state != CAN_STATE_STARTED) return false;
    
    enter_critical(&driver->critical);
    
    bool result = queue_push(&driver->tx_queue, frame, sizeof(CANFrame));
    
    if (result) {
        // Trigger transmission if no active TX
        if (!driver->interrupts.tx_complete) {
            // Load frame into empty mailbox
            // ... write frame data ...
        }
    }
    
    exit_critical(&driver->critical);
    return result;
}

bool can_receive(CANDriver* driver, CANFrame* frame, uint32_t timeout_ms) {
    if (!driver || !frame || driver->state != CAN_STATE_STARTED) return false;
    
    return queue_pop(&driver->rx_queue, frame, sizeof(CANFrame));
}

CANState can_get_state(const CANDriver* driver) {
    return driver ? driver->state : CAN_STATE_UNINIT;
}

CANError can_get_last_error(const CANDriver* driver) {
    return driver ? driver->last_error : CAN_ERROR_NONE;
}

void can_get_statistics(const CANDriver* driver, CANStats* stats) {
    if (!driver || !stats) return;
    
    enter_critical(&driver->critical);
    memcpy(stats, &driver->statistics, sizeof(CANStats));
    exit_critical(&driver->critical);
}

bool can_set_filter(CANDriver* driver, uint32_t id, uint32_t mask, bool is_extended) {
    if (!driver) return false;
    
    enter_critical(&driver->critical);
    
    if (driver->filters.count >= driver->filters.capacity) {
        exit_critical(&driver->critical);
        return false;
    }
    
    size_t idx = driver->filters.count++;
    driver->filters.ids[idx] = id;
    driver->filters.masks[idx] = mask;
    driver->filters.is_extended[idx] = is_extended;
    
    // Update hardware filters
    // ... configure filter registers ...
    
    exit_critical(&driver->critical);
    return true;
}

void can_clear_filters(CANDriver* driver) {
    if (!driver) return;
    
    enter_critical(&driver->critical);
    driver->filters.count = 0;
    // Reset hardware filters
    driver->can_base[CAN_RXMGMASK] = 0xFFFFFFFF;
    exit_critical(&driver->critical);
} 