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
