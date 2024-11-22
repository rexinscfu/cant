add_executable(parser_tests
    unit/parser_tests.c
    ../src/compiler/frontend/parser.c
    ../src/compiler/frontend/lexer.c
    ../src/compiler/middle/ast.c
    ../src/utils/error.c
)

target_include_directories(parser_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME parser_tests COMMAND parser_tests)
