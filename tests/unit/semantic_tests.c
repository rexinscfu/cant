#include <assert.h>
#include <string.h>
#include "../../src/compiler/analysis/semantic.h"
#include "../../src/compiler/frontend/parser.h"

static void test_duplicate_ecu_detection(void) {
    const char* source =
        "ecu MainECU { }\n"
        "ecu MainECU { }";  // Duplicate ECU name
    
    Parser* parser = parser_create(source);
    assert(parser != NULL);
    
    ASTNode* ast = parser_parse(parser);
    assert(ast != NULL);
    
    Analyzer* analyzer = analyzer_create();
    assert(analyzer != NULL);
    
    bool result = analyzer_check(analyzer, ast);
    assert(!result);  // Should fail due to duplicate ECU
    
    DiagnosticBag* diagnostics = analyzer_get_diagnostics(analyzer);
    assert(diagnostics->count > 0);
    assert(strstr(diagnostics->diagnostics[0].message, "Duplicate") != NULL);
    
    analyzer_destroy(analyzer);
    ast_destroy(ast);
    parser_destroy(parser);
}

static void test_valid_signal_definition(void) {
    const char* source =
        "ecu MainECU {\n"
        "    signal EngineSpeed: CAN {\n"
        "        id: 0x100;\n"
        "        length: 16;\n"
        "    }\n"
        "}";
    
    Parser* parser = parser_create(source);
    assert(parser != NULL);
    
    ASTNode* ast = parser_parse(parser);
    assert(ast != NULL);
    
    Analyzer* analyzer = analyzer_create();
    assert(analyzer != NULL);
    
    bool result = analyzer_check(analyzer, ast);
    assert(result);  // Should succeed
    
    DiagnosticBag* diagnostics = analyzer_get_diagnostics(analyzer);
    assert(diagnostics->count == 0);
    
    analyzer_destroy(analyzer);
    ast_destroy(ast);
    parser_destroy(parser);
}

int main(void) {
    test_duplicate_ecu_detection();
    test_valid_signal_definition();
    printf("All semantic analysis tests passed!\n");
    return 0;
} 