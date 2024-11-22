#ifndef CANT_LEXER_H
#define CANT_LEXER_H

#include <stdint.h>

typedef enum {
    TOK_EOF = 0,
    
    // Keywords
    TOK_ECU,
    TOK_SIGNAL,
    TOK_CAN,
    TOK_FLEXRAY,
    
    // Literals
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_IDENTIFIER,
    
    // Special tokens
    TOK_ERROR
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *start;
    size_t length;
    uint32_t line;
    uint32_t column;
} Token;

typedef struct Lexer Lexer;

// Lexer API
Lexer *lexer_create(const char *source);
void lexer_destroy(Lexer *lexer);
Token lexer_get_token(Lexer *lexer);

#endif // CANT_LEXER_H
