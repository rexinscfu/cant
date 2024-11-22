#include <assert.h>
#include "lexer.h"

void test_basic_ecu_definition(void) {
    const char *source = "ecu MainECU { frequency: 200MHz; }";
    Lexer *lexer = lexer_create(source);
    
    assert(lexer_next_token(lexer).kind == TOK_ECU);
    assert(lexer_next_token(lexer).kind == TOK_IDENTIFIER);
    // ... additional assertions ...
    
    lexer_destroy(lexer);
} 