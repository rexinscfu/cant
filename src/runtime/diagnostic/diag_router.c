#include "diag_router.h"
#include "diag_core.h"
#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include <string.h>

// global context - not great but works for now
static DiagRouterContext router;

// debugging stuff
//#define DEBUG_ROUTER
#ifdef DEBUG_ROUTER
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

// forward declarations
static void process_route(const DiagRoute* route, const uint8_t* data, uint32_t length);
static bool validate_message(const uint8_t* data, uint32_t length);

bool DiagRouter_Init(void) {
    if (router.initialized) {
        DBG_PRINT("Router already initialized!\n");
        return false;  // should we allow reinit?
    }
    
    // zero everything - memset is probably overkill but whatever
    memset(&router, 0, sizeof(DiagRouterContext));
    
    // hack: pre-allocate some common routes
    DiagRouter_AddRoute(0x01, 0xF1, 0xFFFF);  // tester -> engine
    DiagRouter_AddRoute(0x01, 0xF2, 0xFFFF);  // tester -> trans
    DiagRouter_AddRoute(0x01, 0xF3, 0xFFFF);  // tester -> body
    
    router.initialized = true;
    return true;
}

void DiagRouter_Deinit(void) {
    // TODO: should we notify active routes?
    clear_routes();
    router.initialized = false;
}

RouteResult DiagRouter_AddRoute(uint8_t source, uint8_t target, uint16_t service) {
    if (!router.initialized) return ROUTE_ERROR;
    
    // sanity check
    if (source == 0 || target == 0) {
        return ROUTE_INVALID_PARAM;
    }
    
    // check if route already exists
    for (uint32_t i = 0; i < router.route_count; i++) {
        DiagRoute* route = &router.routes[i];
        if (route->source_addr == source && 
            route->target_addr == target &&
            route->service_id == service) {
            DBG_PRINT("Route exists: %02X -> %02X (sid=%04X)\n", 
                     source, target, service);
            return ROUTE_OK;  // already exists
        }
    }
    
    // find free slot
    uint32_t idx = router.route_count;
    if (idx >= MAX_ROUTES) {
        DBG_PRINT("No free routes!\n");
        return ROUTE_ERROR;
    }
    
    // add new route
    DiagRoute* route = &router.routes[idx];
    route->source_addr = source;
    route->target_addr = target;
    route->service_id = service;
    route->active = true;
    router.route_count++;
    
    DBG_PRINT("Added route: %02X -> %02X (sid=%04X)\n", 
             source, target, service);
    return ROUTE_OK;
}

RouteResult DiagRouter_RemoveRoute(uint8_t source, uint8_t target) {
    if (!router.initialized) return ROUTE_ERROR;
    
    for (uint32_t i = 0; i < router.route_count; i++) {
        DiagRoute* route = &router.routes[i];
        if (route->source_addr == source && 
            route->target_addr == target) {
            
            // quick and dirty remove - just mark inactive
            route->active = false;
            
            // cleanup if it's the last route
            if (i == router.route_count - 1) {
                router.route_count--;
            }
            
            DBG_PRINT("Removed route: %02X -> %02X\n", source, target);
            return ROUTE_OK;
        }
    }
    
    return ROUTE_ERROR;  // not found
}

RouteResult DiagRouter_HandleMessage(const uint8_t* data, uint32_t length) {
    if (!router.initialized || !data || length == 0) {
        return ROUTE_INVALID_PARAM;
    }
    
    // basic validation
    if (!validate_message(data, length)) {
        DBG_PRINT("Invalid message format!\n");
        return ROUTE_ERROR;
    }
    
    // need at least 3 bytes: source, target, service
    if (length < 3) return ROUTE_ERROR;
    
    uint8_t source = data[0];
    uint8_t target = data[1];
    uint16_t service = data[2];
    
    // find matching route
    bool route_found = false;
    for (uint32_t i = 0; i < router.route_count; i++) {
        DiagRoute* route = &router.routes[i];
        if (!route->active) continue;
        
        if (route->source_addr == source && 
            route->target_addr == target &&
            (route->service_id == 0xFFFF || route->service_id == service)) {
            
            process_route(route, data, length);
            route_found = true;
            // don't break - might have multiple routes
        }
    }
    
    return route_found ? ROUTE_OK : ROUTE_ERROR;
}

// internal stuff
static void process_route(const DiagRoute* route, const uint8_t* data, uint32_t length) {
    // temporary copy to avoid buffer issues
    if (length > TEMP_BUFFER_SIZE) {
        DBG_PRINT("Message too long: %d\n", length);
        return;
    }
    
    memcpy(router.temp_buffer, data, length);
    
    // TODO: add proper network handling
    // for now just pass to core
    DiagCore_HandleMessage(router.temp_buffer, length);
}

static bool validate_message(const uint8_t* data, uint32_t length) {
    // super basic validation for now
    if (data[0] == 0 || data[1] == 0) return false;
    
    // check if we have full message
    // format: [source][target][service][data...][checksum]
    uint32_t min_length = 4;  // source + target + service + checksum
    if (length < min_length) return false;
    
    // TODO: add checksum validation
    return true;
}

void clear_routes(void) {
    memset(router.routes, 0, sizeof(router.routes));
    router.route_count = 0;
}

bool is_route_valid(const DiagRoute* route) {
    if (!route) return false;
    return (route->source_addr != 0 && route->target_addr != 0);
}

// legacy support - probably remove in next version
#if defined(SUPPORT_LEGACY_API)
bool DiagRouter_LegacyInit(OldRouteCallback cb) {
    (void)cb;  // unused
    return DiagRouter_Init();
}
#endif

/*
// old stats code - keep for reference
static RouterStats stats;
void update_stats(void) {
    stats.rx_count++;
    // TODO: add error tracking
}
*/ 