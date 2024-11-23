#include "unity.h"
#include "network/net_core.h"
#include <string.h>

static NetManagerConfig test_config;
static bool callback_triggered;
static NetEventType last_event;

static void test_callback(NetEventType event, void* data, void* context) {
    callback_triggered = true;
    last_event = event;
}

void setUp(void) {
    memset(&test_config, 0, sizeof(NetManagerConfig));
    test_config.max_interfaces = 4;
    test_config.max_connections = 8;
    test_config.rx_buffer_size = 1024;
    test_config.tx_buffer_size = 1024;
    test_config.enable_statistics = true;
    test_config.auto_reconnect = true;
    test_config.heartbeat_interval_ms = 1000;
    
    callback_triggered = false;
    last_event = NET_EVENT_ERROR;
    
    TEST_ASSERT_TRUE(Net_Init(&test_config));
}

void tearDown(void) {
    Net_Deinit();
}

void test_Net_AddRemoveInterface(void) {
    NetInterfaceConfig if_config = {
        .type = NET_IF_ETHERNET,
        .name = "eth0",
        .address = "192.168.1.100",
        .port = 8080,
        .auto_connect = true,
        .reconnect_interval_ms = 5000,
        .timeout_ms = 1000
    };
    
    TEST_ASSERT_TRUE(Net_AddInterface(&if_config));
    TEST_ASSERT_EQUAL(NET_STATE_DISCONNECTED, Net_GetState(NET_IF_ETHERNET));
    TEST_ASSERT_TRUE(Net_RemoveInterface(NET_IF_ETHERNET));
}

void test_Net_ConnectDisconnect(void) {
    NetInterfaceConfig if_config = {
        .type = NET_IF_ETHERNET,
        .name = "eth0",
        .address = "192.168.1.100",
        .port = 8080,
        .auto_connect = false
    };
    
    TEST_ASSERT_TRUE(Net_AddInterface(&if_config));
    TEST_ASSERT_TRUE(Net_Connect(NET_IF_ETHERNET));
    TEST_ASSERT_EQUAL(NET_STATE_CONNECTED, Net_GetState(NET_IF_ETHERNET));
    TEST_ASSERT_TRUE(Net_Disconnect(NET_IF_ETHERNET));
    TEST_ASSERT_EQUAL(NET_STATE_DISCONNECTED, Net_GetState(NET_IF_ETHERNET));
}

void test_Net_SendReceiveMessage(void) {
    const uint8_t test_data[] = "Test Message";
    NetMessage message = {
        .id = 1,
        .data = (uint8_t*)test_data,
        .length = strlen((char*)test_data) + 1,
        .protocol = NET_PROTO_TCP,
        .timestamp = 0,
        .flags = 0
    };
    
    Net_RegisterCallback(NET_EVENT_DATA_SENT, test_callback, NULL);
    TEST_ASSERT_TRUE(Net_SendMessage(&message));
    TEST_ASSERT_TRUE(callback_triggered);
    TEST_ASSERT_EQUAL(NET_EVENT_DATA_SENT, last_event);
} 