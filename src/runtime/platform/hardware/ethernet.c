#include "ethernet.h"
#include "../../diagnostic/logging/diag_logger.h"
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

typedef struct {
    EthernetInit_t config;
    int socket;
    bool initialized;
    bool running;
} EthernetContext;

static EthernetContext eth_ctx;

bool Ethernet_Init(const EthernetInit_t* params) {
    if (!params) {
        return false;
    }

    memset(&eth_ctx, 0, sizeof(EthernetContext));
    memcpy(&eth_ctx.config, params, sizeof(EthernetInit_t));

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Logger_Log(LOG_LEVEL_ERROR, "ETH", "WSAStartup failed");
        return false;
    }
#endif

    eth_ctx.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (eth_ctx.socket < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "ETH", "Socket creation failed");
        return false;
    }

    eth_ctx.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "ETH", "Ethernet initialized");
    return true;
}

void Ethernet_Deinit(void) {
    if (!eth_ctx.initialized) {
        return;
    }

    if (eth_ctx.running) {
        Ethernet_Stop();
    }

#ifdef _WIN32
    closesocket(eth_ctx.socket);
    WSACleanup();
#else
    close(eth_ctx.socket);
#endif

    memset(&eth_ctx, 0, sizeof(EthernetContext));
    Logger_Log(LOG_LEVEL_INFO, "ETH", "Ethernet deinitialized");
}

bool Ethernet_Start(void) {
    if (!eth_ctx.initialized || eth_ctx.running) {
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    
    if (eth_ctx.config.dhcp_enabled) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(eth_ctx.config.static_ip);
    }

    if (bind(eth_ctx.socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "ETH", "Bind failed");
        return false;
    }

    eth_ctx.running = true;
    Logger_Log(LOG_LEVEL_INFO, "ETH", "Ethernet started");
    return true;
}

bool Ethernet_Stop(void) {
    if (!eth_ctx.initialized || !eth_ctx.running) {
        return false;
    }

    eth_ctx.running = false;
    Logger_Log(LOG_LEVEL_INFO, "ETH", "Ethernet stopped");
    return true;
}

bool Ethernet_GetStatus(uint32_t* link_speed, bool* link_up) {
    if (!eth_ctx.initialized || !link_speed || !link_up) {
        return false;
    }

    *link_speed = 100; // Assuming 100Mbps
    *link_up = eth_ctx.running;
    return true;
}

bool Ethernet_Send(const uint8_t* data, uint32_t length) {
    if (!eth_ctx.initialized || !eth_ctx.running || !data || length == 0) {
        return false;
    }

    int sent = send(eth_ctx.socket, (const char*)data, length, 0);
    if (sent < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "ETH", "Send failed");
        return false;
    }

    return (uint32_t)sent == length;
}

bool Ethernet_Receive(uint8_t* data, uint32_t* length) {
    if (!eth_ctx.initialized || !eth_ctx.running || !data || !length || *length == 0) {
        return false;
    }

    int received = recv(eth_ctx.socket, (char*)data, *length, 0);
    if (received < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "ETH", "Receive failed");
        return false;
    }

    *length = received;
    return true;
} 