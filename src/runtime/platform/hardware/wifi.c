#include "wifi.h"
#include "../../diagnostic/logging/diag_logger.h"
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

typedef struct {
    WiFiInit_t config;
    int socket;
    bool initialized;
    bool connected;
    int8_t current_signal;
    uint8_t current_channel;
} WiFiContext;

static WiFiContext wifi_ctx;

bool WiFi_Init(const WiFiInit_t* params) {
    if (!params) {
        return false;
    }

    memset(&wifi_ctx, 0, sizeof(WiFiContext));
    memcpy(&wifi_ctx.config, params, sizeof(WiFiInit_t));

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Logger_Log(LOG_LEVEL_ERROR, "WIFI", "WSAStartup failed");
        return false;
    }
#endif

    wifi_ctx.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (wifi_ctx.socket < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "WIFI", "Socket creation failed");
        return false;
    }

    wifi_ctx.initialized = true;
    wifi_ctx.current_signal = -90; // Initial signal strength
    wifi_ctx.current_channel = 1;  // Initial channel

    Logger_Log(LOG_LEVEL_INFO, "WIFI", "WiFi initialized with SSID: %s", params->ssid);
    return true;
}

void WiFi_Deinit(void) {
    if (!wifi_ctx.initialized) {
        return;
    }

    if (wifi_ctx.connected) {
        WiFi_Disconnect();
    }

#ifdef _WIN32
    closesocket(wifi_ctx.socket);
    WSACleanup();
#else
    close(wifi_ctx.socket);
#endif

    memset(&wifi_ctx, 0, sizeof(WiFiContext));
    Logger_Log(LOG_LEVEL_INFO, "WIFI", "WiFi deinitialized");
}

bool WiFi_Connect(void) {
    if (!wifi_ctx.initialized || wifi_ctx.connected) {
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    if (wifi_ctx.config.use_dhcp) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(wifi_ctx.config.static_ip);
    }

    if (bind(wifi_ctx.socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "WIFI", "Bind failed");
        return false;
    }

    wifi_ctx.connected = true;
    wifi_ctx.current_signal = -65; // Simulated good signal strength
    Logger_Log(LOG_LEVEL_INFO, "WIFI", "Connected to SSID: %s", wifi_ctx.config.ssid);
    return true;
}

bool WiFi_Disconnect(void) {
    if (!wifi_ctx.initialized || !wifi_ctx.connected) {
        return false;
    }

    wifi_ctx.connected = false;
    wifi_ctx.current_signal = -90;
    Logger_Log(LOG_LEVEL_INFO, "WIFI", "Disconnected from SSID: %s", 
               wifi_ctx.config.ssid);
    return true;
}

bool WiFi_GetStatus(int8_t* signal_strength, uint8_t* channel) {
    if (!wifi_ctx.initialized || !signal_strength || !channel) {
        return false;
    }

    *signal_strength = wifi_ctx.current_signal;
    *channel = wifi_ctx.current_channel;
    return true;
}

bool WiFi_Send(const uint8_t* data, uint32_t length) {
    if (!wifi_ctx.initialized || !wifi_ctx.connected || !data || length == 0) {
        return false;
    }

    int sent = send(wifi_ctx.socket, (const char*)data, length, 0);
    if (sent < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "WIFI", "Send failed");
        return false;
    }

    return (uint32_t)sent == length;
}

bool WiFi_Receive(uint8_t* data, uint32_t* length) {
    if (!wifi_ctx.initialized || !wifi_ctx.connected || !data || !length || 
        *length == 0) {
        return false;
    }

    int received = recv(wifi_ctx.socket, (char*)data, *length, 0);
    if (received < 0) {
        Logger_Log(LOG_LEVEL_ERROR, "WIFI", "Receive failed");
        return false;
    }

    *length = received;
    return true;
} 