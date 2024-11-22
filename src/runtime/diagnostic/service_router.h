#ifndef CANT_SERVICE_ROUTER_H
#define CANT_SERVICE_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "uds_handler.h"

// Service Handler Function Type
typedef UdsResponseCode (*ServiceHandler)(const UdsMessage* request, UdsMessage* response);

// Service Route Definition
typedef struct {
    UdsServiceId service_id;
    ServiceHandler handler;
    bool requires_security;
    uint8_t min_security_level;
} ServiceRoute;

// Service Router Configuration
typedef struct {
    ServiceRoute* routes;
    uint32_t route_count;
    void (*pre_process_callback)(const UdsMessage* request);
    void (*post_process_callback)(const UdsMessage* request, const UdsMessage* response);
} ServiceRouterConfig;

// Service Router API
bool Service_Router_Init(const ServiceRouterConfig* config);
void Service_Router_DeInit(void);
UdsResponseCode Service_Router_ProcessRequest(const UdsMessage* request, UdsMessage* response);
bool Service_Router_AddRoute(const ServiceRoute* route);
bool Service_Router_RemoveRoute(UdsServiceId service_id);
ServiceRoute* Service_Router_GetRoute(UdsServiceId service_id);
uint32_t Service_Router_GetRouteCount(void);

#endif // CANT_SERVICE_ROUTER_H 