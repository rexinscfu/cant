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
    
    // Diagnostic Keywords
    TOK_DIAGNOSTIC,
    TOK_SERVICE,
    TOK_SESSION,
    TOK_SECURITY,
    TOK_REQUEST,
    TOK_RESPONSE,
    TOK_ROUTINE,
    TOK_DID,
    TOK_PATTERN,
    TOK_MATCH,
    TOK_TIMEOUT,
    TOK_FLOW,
    
    // CAN Frame Keywords
    TOK_FRAME,
    TOK_ID,
    TOK_DLC,
    TOK_EXTENDED,
    TOK_PERIODIC,
    TOK_TRIGGER,
    
    // Literals
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_IDENTIFIER,
    TOK_HEX,
    TOK_BINARY,
    TOK_STRING,
    
    // Operators
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_AMPERSAND,
    TOK_PIPE,
    TOK_CARET,
    TOK_TILDE,
    TOK_SHIFT_LEFT,
    TOK_SHIFT_RIGHT,
    
    // Comparison
    TOK_EQUAL,
    TOK_NOT_EQUAL,
    TOK_LESS,
    TOK_LESS_EQUAL,
    TOK_GREATER,
    TOK_GREATER_EQUAL,
    
    // Punctuation
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_DOT,
    TOK_COMMA,
    TOK_ARROW,
    
    // Special tokens
    TOK_ERROR,
    TOK_COMMENT
} TokenKind;

typedef struct {
    TokenKind kind;
    const char* start;
    size_t length;
    uint32_t line;
    uint32_t column;
    union {
        uint64_t int_value;
        double float_value;
        const char* string_value;
    } value;
} Token;

typedef struct Lexer Lexer;

// Lexer API
Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
const char* lexer_token_kind_name(TokenKind kind);
bool lexer_is_keyword(const char* identifier, size_t length);

#endif // CANT_LEXER_H
