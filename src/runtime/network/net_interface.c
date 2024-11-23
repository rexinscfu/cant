#include "net_interface.h"
#include "../diagnostic/logging/diag_logger.h"
#include "../diagnostic/os/critical.h"
#include "../platform/hardware/ethernet.h"
#include "../platform/hardware/wifi.h"
#include "../platform/hardware/cellular.h"
#include "../platform/hardware/can.h"
#include <string.h>

// Interface contexts
static EthernetConfig ethernet_config;
static WiFiConfig wifi_config;
static CellularConfig cellular_config;
static CANInterfaceConfig can_config;

bool NetInterface_ConnectEthernet(const NetInterfaceConfig* config) {
    if (!config || !config->interface_config) {
        return false;
    }

    enter_critical();
    
    EthernetConfig* eth_config = (EthernetConfig*)config->interface_config;
    memcpy(&ethernet_config, eth_config, sizeof(EthernetConfig));

    // Initialize Ethernet hardware
    EthernetInit_t init_params = {
        .mac_address = ethernet_config.mac_address,
        .dhcp_enabled = ethernet_config.dhcp_enabled,
        .static_ip = ethernet_config.static_ip,
        .subnet_mask = ethernet_config.subnet_mask,
        .gateway = ethernet_config.gateway,
        .dns_server = ethernet_config.dns_server
    };

    bool result = Ethernet_Init(&init_params);
    if (result) {
        result = Ethernet_Start();
    }

    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "Ethernet interface connected");
    } else {
        Logger_Log(LOG_LEVEL_ERROR, "NETIF", "Failed to connect Ethernet interface");
    }

    return result;
}

bool NetInterface_DisconnectEthernet(const NetInterfaceConfig* config) {
    enter_critical();
    bool result = Ethernet_Stop();
    Ethernet_Deinit();
    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "Ethernet interface disconnected");
    }
    return result;
}

bool NetInterface_ConnectWiFi(const NetInterfaceConfig* config) {
    if (!config || !config->interface_config) {
        return false;
    }

    enter_critical();
    
    WiFiConfig* wifi_cfg = (WiFiConfig*)config->interface_config;
    memcpy(&wifi_config, wifi_cfg, sizeof(WiFiConfig));

    // Initialize WiFi hardware
    WiFiInit_t init_params = {
        .ssid = wifi_config.ssid,
        .password = wifi_config.password,
        .security_type = wifi_config.security_type,
        .use_dhcp = wifi_config.use_dhcp,
        .static_ip = wifi_config.static_ip,
        .subnet_mask = wifi_config.subnet_mask,
        .gateway = wifi_config.gateway
    };

    bool result = WiFi_Init(&init_params);
    if (result) {
        result = WiFi_Connect();
    }

    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "WiFi interface connected to %s", 
                  wifi_config.ssid);
    } else {
        Logger_Log(LOG_LEVEL_ERROR, "NETIF", "Failed to connect WiFi interface");
    }

    return result;
}

bool NetInterface_DisconnectWiFi(const NetInterfaceConfig* config) {
    enter_critical();
    bool result = WiFi_Disconnect();
    WiFi_Deinit();
    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "WiFi interface disconnected");
    }
    return result;
}

bool NetInterface_ConnectCellular(const NetInterfaceConfig* config) {
    if (!config || !config->interface_config) {
        return false;
    }

    enter_critical();
    
    CellularConfig* cell_cfg = (CellularConfig*)config->interface_config;
    memcpy(&cellular_config, cell_cfg, sizeof(CellularConfig));

    // Initialize Cellular hardware
    CellularInit_t init_params = {
        .apn = cellular_config.apn,
        .username = cellular_config.username,
        .password = cellular_config.password,
        .network_type = cellular_config.network_type,
        .roaming_enabled = cellular_config.roaming_enabled,
        .connection_timeout = cellular_config.connection_timeout
    };

    bool result = Cellular_Init(&init_params);
    if (result) {
        result = Cellular_Connect();
    }

    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "Cellular interface connected to %s", 
                  cellular_config.apn);
    } else {
        Logger_Log(LOG_LEVEL_ERROR, "NETIF", "Failed to connect Cellular interface");
    }

    return result;
}

bool NetInterface_DisconnectCellular(const NetInterfaceConfig* config) {
    enter_critical();
    bool result = Cellular_Disconnect();
    Cellular_Deinit();
    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "Cellular interface disconnected");
    }
    return result;
}

bool NetInterface_ConnectCAN(const NetInterfaceConfig* config) {
    if (!config || !config->interface_config) {
        return false;
    }

    enter_critical();
    
    CANInterfaceConfig* can_cfg = (CANInterfaceConfig*)config->interface_config;
    memcpy(&can_config, can_cfg, sizeof(CANInterfaceConfig));

    // Initialize CAN hardware
    CANInit_t init_params = {
        .id = can_config.id,
        .bitrate = can_config.bitrate,
        .extended_id = can_config.extended_id,
        .fd_enabled = can_config.fd_enabled,
        .data_bitrate = can_config.data_bitrate
    };

    bool result = CAN_Init(&init_params);
    if (result) {
        result = CAN_Start();
    }

    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "CAN interface connected");
    } else {
        Logger_Log(LOG_LEVEL_ERROR, "NETIF", "Failed to connect CAN interface");
    }

    return result;
}

bool NetInterface_DisconnectCAN(const NetInterfaceConfig* config) {
    enter_critical();
    bool result = CAN_Stop();
    CAN_Deinit();
    exit_critical();

    if (result) {
        Logger_Log(LOG_LEVEL_INFO, "NETIF", "CAN interface disconnected");
    }
    return result;
}

bool NetInterface_GetEthernetStatus(uint32_t* link_speed, bool* link_up) {
    if (!link_speed || !link_up) {
        return false;
    }

    enter_critical();
    bool result = Ethernet_GetStatus(link_speed, link_up);
    exit_critical();

    return result;
}

bool NetInterface_GetWiFiStatus(int8_t* signal_strength, uint8_t* channel) {
    if (!signal_strength || !channel) {
        return false;
    }

    enter_critical();
    bool result = WiFi_GetStatus(signal_strength, channel);
    exit_critical();

    return result;
}

bool NetInterface_GetCellularStatus(int8_t* signal_strength, uint8_t* network_type) {
    if (!signal_strength || !network_type) {
        return false;
    }

    enter_critical();
    bool result = Cellular_GetStatus(signal_strength, network_type);
    exit_critical();

    return result;
}

bool NetInterface_GetCANStatus(uint32_t* error_count, bool* bus_off) {
    if (!error_count || !bus_off) {
        return false;
    }

    enter_critical();
    bool result = CAN_GetStatus(error_count, bus_off);
    exit_critical();

    return result;
} 