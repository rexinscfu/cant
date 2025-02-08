add_library(diagnostic_config STATIC
    diagnostic_config.c
)

target_include_directories(diagnostic_config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/runtime/diagnostic
        ${CMAKE_SOURCE_DIR}/src/runtime/utils
)

target_link_libraries(diagnostic_config
    PRIVATE
        diagnostic_core
        utils
) 