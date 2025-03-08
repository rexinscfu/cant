add_executable(semantic_tests
    unit/semantic_tests.c
    ../src/compiler/analysis/semantic.c
    ../src/compiler/middle/types.c
    ../src/compiler/frontend/parser.c
    ../src/compiler/frontend/lexer.c
    ../src/compiler/middle/ast.c
    ../src/utils/error.c
)

target_include_directories(semantic_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME semantic_tests COMMAND semantic_tests)

# Add CAN validation tests
add_executable(can_validation_tests
    unit/can_validation_tests.c
    ../src/compiler/automotive/can_validation.c
)

target_include_directories(can_validation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(can_validation_tests m)  # Link math library

add_test(NAME can_validation_tests COMMAND can_validation_tests)

# Add signal analyzer tests
add_executable(signal_analyzer_tests
    unit/signal_analyzer_tests.c
    ../tools/dev_utils/signal_analyzer.c
)

target_include_directories(signal_analyzer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tools
)

target_link_libraries(signal_analyzer_tests m)

add_test(NAME signal_analyzer_tests COMMAND signal_analyzer_tests)

# Add real-time scheduler tests
add_executable(rt_scheduler_tests
    unit/rt_scheduler_tests.c
    ../src/runtime/scheduler/rt_scheduler.c
    ../src/runtime/memory/rt_memory.c
    ../src/runtime/watchdog/watchdog.c
)

target_include_directories(rt_scheduler_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(rt_scheduler_tests
    pthread
    rt
)

add_test(NAME rt_scheduler_tests COMMAND rt_scheduler_tests)

# Add LLVM generator tests
add_executable(llvm_generator_tests
    unit/llvm_generator_tests.c
)

target_link_libraries(llvm_generator_tests
    cant_backend
    cant_middle
    cant_utils
)

add_test(NAME llvm_generator_tests COMMAND llvm_generator_tests)
