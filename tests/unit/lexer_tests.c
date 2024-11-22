#include <assert.h>
#include <string.h>
#include "../../src/compiler/frontend/lexer.h"

static void test_basic_tokens(void) {
    const char* source = "ecu MainECU { frequency: 200MHz; }";
    Lexer* lexer = lexer_create(source);
    assert(lexer != NULL);

    Token token = lexer_next_token(lexer);
    assert(token.kind == TOK_ECU);

    token = lexer_next_token(lexer);
    assert(token.kind == TOK_IDENTIFIER);
    assert(token.length == 7);
    assert(strncmp(token.start, "MainECU", token.length) == 0);

    token = lexer_next_token(lexer);
    assert(token.kind == TOK_LBRACE);

    lexer_destroy(lexer);
}

static void test_numbers(void) {
    const char* source = "123 456.789";
    Lexer* lexer = lexer_create(source);
    assert(lexer != NULL);

    Token token = lexer_next_token(lexer);
    assert(token.kind == TOK_INTEGER);
    assert(token.length == 3);

    token = lexer_next_token(lexer);
    assert(token.kind == TOK_INTEGER);
    assert(token.length == 7);

    lexer_destroy(lexer);
}

int main(void) {
    test_basic_tokens();
    test_numbers();
    printf("All lexer tests passed!\n");
    return 0;
} 