#include <gtest/gtest.h>
#include "../diag_system.h"
#include "mocks/ecu_mock.h"
#include "mocks/network_mock.h"

class DiagErrorInjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ECU_Mock_Init();
        Network_Mock_Init();
    }

    void TearDown() override {
        ECU_Mock_Deinit();
        Network_Mock_Deinit();
    }
};

TEST_F(DiagErrorInjectionTest, CommunicationErrors) {
    // Test network disconnection during service
    Network_Mock_SetNetworkStatus(NETWORK_STATUS_DISCONNECTED);
    
    uint8_t request[] = {0x22, 0xF1, 0x90}; // Read VIN
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(request, sizeof(request), &response),
              UDS_RESPONSE_CONDITIONS_NOT_CORRECT);

    // Test network timeout
    Network_Mock_SetNetworkStatus(NETWORK_STATUS_CONNECTED);
    Network_Mock_SetResponseTimes((uint16_t[]){0xFFFF}, 1); // Simulate timeout
    ASSERT_EQ(Diag_System_HandleRequest(request, sizeof(request), &response),
              UDS_RESPONSE_TIMEOUT);
}

TEST_F(DiagErrorInjectionTest, ResourceExhaustion) {
    // Simulate memory allocation failure
    Memory_Mock_SetMemoryExhausted(true);
    
    uint8_t request[] = {0x2E, 0xF1, 0x90, 0x11, 0x22, 0x33}; // Write data
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(request, sizeof(request), &response),
              UDS_RESPONSE_CONDITIONS_NOT_CORRECT);
}

TEST_F(DiagErrorInjectionTest, ConcurrentAccessConflicts) {
    // Simulate routine already running
    uint8_t start_routine[] = {0x31, 0x01, 0x01, 0x00}; // Start routine
    UdsMessage response;
    
    ASSERT_EQ(Diag_System_HandleRequest(start_routine, sizeof(start_routine), &response),
              UDS_RESPONSE_OK);
              
    // Try to start same routine again
    ASSERT_EQ(Diag_System_HandleRequest(start_routine, sizeof(start_routine), &response),
              UDS_RESPONSE_CONDITIONS_NOT_CORRECT);
} 