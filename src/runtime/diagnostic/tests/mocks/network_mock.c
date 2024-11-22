#include "network_mock.h"
#include <string.h>

#define MAX_NETWORK_NODES 32

static struct {
    uint32_t baudrate;
    uint8_t node_address;
    uint8_t active_node_count;
    uint16_t response_times[MAX_NETWORK_NODES];
    uint8_t error_counts[MAX_NETWORK_NODES];
    bool test_complete;
    uint8_t network_status;
} network_mock_data;

void Network_Mock_Init(void) {
    memset(&network_mock_data, 0, sizeof(network_mock_data));
    network_mock_data.baudrate = 500000;
    network_mock_data.node_address = 0x01;
    network_mock_data.network_status = 0x00;
}

void Network_Mock_Deinit(void) {
    memset(&network_mock_data, 0, sizeof(network_mock_data));
}

void Network_Mock_SetBaudrate(uint32_t baudrate) {
    network_mock_data.baudrate = baudrate;
}

void Network_Mock_SetNodeAddress(uint8_t address) {
    network_mock_data.node_address = address;
}

void Network_Mock_SetActiveNodeCount(uint8_t count) {
    network_mock_data.active_node_count = count;
}

void Network_Mock_SetResponseTimes(const uint16_t* times) {
    memcpy(network_mock_data.response_times, times, 
           sizeof(uint16_t) * MAX_NETWORK_NODES);
}

void Network_Mock_SetErrorCounts(const uint8_t* errors) {
    memcpy(network_mock_data.error_counts, errors, MAX_NETWORK_NODES);
}

void Network_Mock_SetTestComplete(bool complete) {
    network_mock_data.test_complete = complete;
}

void Network_Mock_SetNetworkStatus(uint8_t status) {
    network_mock_data.network_status = status;
}

// Getter implementations
uint32_t Network_Mock_GetBaudrate(void) {
    return network_mock_data.baudrate;
}

uint8_t Network_Mock_GetNodeAddress(void) {
    return network_mock_data.node_address;
}

uint8_t Network_Mock_GetActiveNodeCount(void) {
    return network_mock_data.active_node_count;
}

void Network_Mock_GetResponseTimes(uint16_t* times) {
    memcpy(times, network_mock_data.response_times, 
           sizeof(uint16_t) * MAX_NETWORK_NODES);
}

void Network_Mock_GetErrorCounts(uint8_t* errors) {
    memcpy(errors, network_mock_data.error_counts, MAX_NETWORK_NODES);
}

bool Network_Mock_IsTestComplete(void) {
    return network_mock_data.test_complete;
}

uint8_t Network_Mock_GetNetworkStatus(void) {
    return network_mock_data.network_status;
} 