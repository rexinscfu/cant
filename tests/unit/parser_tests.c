#include <assert.h>
#include <string.h>
#include "../../src/compiler/frontend/parser.h"

static void test_basic_ecu_parsing(void) {
    const char* source = "ecu MainECU { }";
    Parser* parser = parser_create(source);
    assert(parser != NULL);
    
    ASTNode* ast = parser_parse(parser);
    assert(ast != NULL);
    assert(ast->type == AST_ECU_DEF);
    assert(ast->as.ecu_def.identifier != NULL);
    assert(strcmp(ast->as.ecu_def.identifier->as.identifier.name, "MainECU") == 0);
    
    ast_destroy(ast);
    parser_destroy(parser);
}

static void test_parser_error_handling(void) {
    const char* source = "ecu { }"; // Missing identifier
    Parser* parser = parser_create(source);
    assert(parser != NULL);
    
    ASTNode* ast = parser_parse(parser);
    assert(ast == NULL);
    
    ParseError error = parser_get_error(parser);
    assert(error.message != NULL);
    assert(strstr(error.message, "identifier") != NULL);
    
    parser_destroy(parser);
}

int main(void) {
    test_basic_ecu_parsing();
    test_parser_error_handling();
    printf("All parser tests passed!\n");
    return 0;
} 