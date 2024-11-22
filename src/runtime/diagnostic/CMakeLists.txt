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