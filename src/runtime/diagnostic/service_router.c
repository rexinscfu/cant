#include "service_router.h"
#include <string.h>
#include "../os/critical.h"
#include "session_manager.h"

#define MAX_ROUTES 50

typedef struct {
    ServiceRouterConfig config;
    ServiceRoute routes[MAX_ROUTES];
    uint32_t route_count;
    bool initialized;
    CriticalSection critical;
} ServiceRouter;

static ServiceRouter service_router;

static ServiceRoute* find_route(UdsServiceId service_id) {
    for (uint32_t i = 0; i < service_router.route_count; i++) {
        if (service_router.routes[i].service_id == service_id) {
            return &service_router.routes[i];
        }
    }
    return NULL;
}

bool Service_Router_Init(const ServiceRouterConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&service_router.critical);

    memcpy(&service_router.config, config, sizeof(ServiceRouterConfig));
    
    // Copy initial routes if provided
    if (config->routes && config->route_count > 0) {
        uint32_t copy_count = (config->route_count <= MAX_ROUTES) ? 
                             config->route_count : MAX_ROUTES;
        memcpy(service_router.routes, config->routes, 
               sizeof(ServiceRoute) * copy_count);
        service_router.route_count = copy_count;
    } else {
        service_router.route_count = 0;
    }

    init_critical(&service_router.critical);
    service_router.initialized = true;

    exit_critical(&service_router.critical);
    return true;
}

void Service_Router_DeInit(void) {
    enter_critical(&service_router.critical);
    memset(&service_router, 0, sizeof(ServiceRouter));
    exit_critical(&service_router.critical);
}

UdsResponseCode Service_Router_ProcessRequest(const UdsMessage* request, UdsMessage* response) {
    if (!service_router.initialized || !request || !response) {
        return UDS_RESPONSE_GENERAL_REJECT;
    }

    enter_critical(&service_router.critical);

    // Pre-process callback if configured
    if (service_router.config.pre_process_callback) {
        service_router.config.pre_process_callback(request);
    }

    // Find route for the service
    ServiceRoute* route = find_route(request->service_id);
    if (!route) {
        exit_critical(&service_router.critical);
        return UDS_RESPONSE_SERVICE_NOT_SUPPORTED;
    }

    // Check session and security access
    if (!Session_Manager_IsServiceAllowed(request->service_id)) {
        exit_critical(&service_router.critical);
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    if (route->requires_security) {
        SessionState session_state = Session_Manager_GetState();
        if (session_state.security_level < route->min_security_level) {
            exit_critical(&service_router.critical);
            return UDS_RESPONSE_SECURITY_ACCESS_DENIED;
        }
    }

    // Execute service handler
    UdsResponseCode result = UDS_RESPONSE_GENERAL_REJECT;
    if (route->handler) {
        result = route->handler(request, response);
    }

    // Post-process callback if configured
    if (service_router.config.post_process_callback) {
        service_router.config.post_process_callback(request, response);
    }

    exit_critical(&service_router.critical);
    return result;
}

bool Service_Router_AddRoute(const ServiceRoute* route) {
    if (!service_router.initialized || !route) {
        return false;
    }

    enter_critical(&service_router.critical);

    // Check if route already exists
    if (find_route(route->service_id)) {
        exit_critical(&service_router.critical);
        return false;
    }

    // Check if we have space for new route
    if (service_router.route_count >= MAX_ROUTES) {
        exit_critical(&service_router.critical);
        return false;
    }

    // Add new route
    memcpy(&service_router.routes[service_router.route_count], route, 
           sizeof(ServiceRoute));
    service_router.route_count++;

    exit_critical(&service_router.critical);
    return true;
}

bool Service_Router_RemoveRoute(UdsServiceId service_id) {
    if (!service_router.initialized) {
        return false;
    }

    enter_critical(&service_router.critical);

    // Find route index
    int32_t route_index = -1;
    for (uint32_t i = 0; i < service_router.route_count; i++) {
        if (service_router.routes[i].service_id == service_id) {
            route_index = i;
            break;
        }
    }

    if (route_index < 0) {
        exit_critical(&service_router.critical);
        return false;
    }

    // Remove route by shifting remaining routes
    if (route_index < (service_router.route_count - 1)) {
        memmove(&service_router.routes[route_index],
                &service_router.routes[route_index + 1],
                sizeof(ServiceRoute) * (service_router.route_count - route_index - 1));
    }

    service_router.route_count--;

    exit_critical(&service_router.critical);
    return true;
}

ServiceRoute* Service_Router_GetRoute(UdsServiceId service_id) {
    if (!service_router.initialized) {
        return NULL;
    }

    enter_critical(&service_router.critical);
    ServiceRoute* route = find_route(service_id);
    exit_critical(&service_router.critical);

    return route;
}

uint32_t Service_Router_GetRouteCount(void) {
    if (!service_router.initialized) {
        return 0;
    }
    return service_router.route_count;
} 