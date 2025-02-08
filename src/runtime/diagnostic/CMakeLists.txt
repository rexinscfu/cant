set(DIAG_SOURCES
    diag_system.c
    diag_transport.c
    memory_manager.c
    session_manager.c
    service_router.c
    data_manager.c
    security_manager.c
    routine_manager.c
    comm_manager.c
)

set(DIAG_HEADERS
    diag_system.h
    diag_transport.h
    memory_manager.h
    session_manager.h
    service_router.h
    data_manager.h
    security_manager.h
    routine_manager.h
    comm_manager.h
)

add_library(diagnostic STATIC
    ${DIAG_SOURCES}
    ${DIAG_HEADERS}
)

target_include_directories(diagnostic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/src/runtime/os
        ${CMAKE_SOURCE_DIR}/src/runtime/utils
)

target_link_libraries(diagnostic
    PRIVATE
        os
        utils
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

target_sources(cant_runtime
    PRIVATE
        security_manager.c
        security_state.c
        service_router.c
        session_manager.c
        uds_handler.c
        diag_state.c
        diag_system.c
        diag_timer.c
        diag_transport.c
        dtc_manager.c
        event_handler.c
        memory_manager.c
        perf_monitor.c
        route_table.c
        routine_manager.c
        comm_manager.c
        data_manager.c
        diag_core.c
        diag_error.c
        diag_filter.c
        diag_logger.c
        diag_monitor.c
        diag_parser.c
        diag_protocol.c
        diag_recorder.c
        diag_response.c
        diag_router.c
        diag_security.c
        diag_session.c
)

target_include_directories(cant_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
) 