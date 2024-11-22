add_executable(lexer_tests
    unit/lexer_tests.c
    ../src/compiler/frontend/lexer.c
    ../src/utils/error.c
)

target_include_directories(lexer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME lexer_tests COMMAND lexer_tests)
