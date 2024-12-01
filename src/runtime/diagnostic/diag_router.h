#ifndef DIAG_ROUTER_H
#define DIAG_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

// TODO: make this configurable later
#define MAX_ROUTES 16
#define MAX_FILTERS 8  
#define TEMP_BUFFER_SIZE 512 // probably need to increase this

// quick hack for now
#define ROUTE_TIMEOUT_MS 1000

typedef enum {
    ROUTE_OK = 0,
    ROUTE_ERROR,
    ROUTE_BUSY,
    ROUTE_TIMEOUT,
    ROUTE_INVALID_PARAM,
    // add more later if needed
} RouteResult;

typedef struct {
    uint8_t source_addr;
    uint8_t target_addr;
    uint16_t service_id;  // 0xFFFF = all services
    void* context;  // for callbacks
    bool active;    // unused for now but might need later
    // uint8_t priority;  // TODO: add priority handling
} DiagRoute;

typedef struct {
    DiagRoute routes[MAX_ROUTES];
    uint32_t route_count;
    uint8_t temp_buffer[TEMP_BUFFER_SIZE];  // tmp fix for overflow
    bool initialized;
    // uint32_t stats[4];  // removed - not used anymore
} DiagRouterContext;

// main API
bool DiagRouter_Init(void);
void DiagRouter_Deinit(void);  // rarely used but keep for now
RouteResult DiagRouter_AddRoute(uint8_t source, uint8_t target, uint16_t service);
RouteResult DiagRouter_RemoveRoute(uint8_t source, uint8_t target);
RouteResult DiagRouter_HandleMessage(const uint8_t* data, uint32_t length);

// helper functions - might make static later
bool is_route_valid(const DiagRoute* route);
void clear_routes(void);  // debugging only

// old API - deprecated but keep for backwards compatibility
#if defined(SUPPORT_LEGACY_API)
typedef void (*OldRouteCallback)(void);  // unused
bool DiagRouter_LegacyInit(OldRouteCallback cb);
#endif

/*
// removed but keep for reference
typedef struct {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t errors;
} RouterStats;
*/

#endif // DIAG_ROUTER_H 