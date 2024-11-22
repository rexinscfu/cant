#ifndef CANT_LEXER_H
#define CANT_LEXER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TOK_EOF = 0,
    
    // Keywords
    TOK_ECU,
    TOK_SIGNAL,
    TOK_CAN,
    TOK_FLEXRAY,
    TOK_PROCESS,
    TOK_INPUT,
    TOK_OUTPUT,
    TOK_FILTER,
    
    // Literals
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_IDENTIFIER,
    
    // Punctuation
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_DOT,
    
    // Special tokens
    TOK_ERROR
} TokenKind;

typedef struct {
    TokenKind kind;
    const char* start;
    size_t length;
    uint32_t line;
    uint32_t column;
} Token;

typedef struct Lexer Lexer;

// Lexer API
Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);

#endif // CANT_LEXER_H
