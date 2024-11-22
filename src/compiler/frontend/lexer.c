#include "lexer.h"
#include "../utils/error.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Lexer {
    const char* source;
    const char* current;
    uint32_t line;
    uint32_t column;
    Error error;
};

// Keyword mapping
static struct {
    const char* keyword;
    TokenKind kind;
} keywords[] = {
    {"ecu", TOK_ECU},
    {"signal", TOK_SIGNAL},
    {"can", TOK_CAN},
    {"flexray", TOK_FLEXRAY},
    {"process", TOK_PROCESS},
    {"input", TOK_INPUT},
    {"output", TOK_OUTPUT},
    {"filter", TOK_FILTER},
    {NULL, TOK_EOF}
};

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_alphanumeric(char c) {
    return is_alpha(c) || is_digit(c);
}

static TokenKind check_keyword(const char* start, size_t length) {
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strlen(keywords[i].keyword) == length &&
            strncmp(keywords[i].keyword, start, length) == 0) {
            return keywords[i].kind;
        }
    }
    return TOK_IDENTIFIER;
}

Lexer* lexer_create(const char* source) {
    Lexer* lexer = malloc(sizeof(Lexer));
    if (lexer == NULL) return NULL;

    lexer->source = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->error.code = ERROR_NONE;
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    free(lexer);
}

static void skip_whitespace(Lexer* lexer) {
    while (*lexer->current != '\0') {
        switch (*lexer->current) {
            case ' ':
            case '\t':
            case '\r':
                lexer->column++;
                lexer->current++;
                break;
            case '\n':
                lexer->line++;
                lexer->column = 1;
                lexer->current++;
                break;
            case '/':
                if (lexer->current[1] == '/') {
                    // Skip line comment
                    lexer->current += 2;
                    while (*lexer->current != '\0' && *lexer->current != '\n') {
                        lexer->current++;
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);

    Token token;
    token.start = lexer->current;
    token.line = lexer->line;
    token.column = lexer->column;

    if (*lexer->current == '\0') {
        token.kind = TOK_EOF;
        token.length = 0;
        return token;
    }

    char c = *lexer->current++;
    lexer->column++;

    if (is_alpha(c)) {
        // Handle identifiers and keywords
        while (is_alphanumeric(*lexer->current)) {
            lexer->current++;
            lexer->column++;
        }
        token.length = lexer->current - token.start;
        token.kind = check_keyword(token.start, token.length);
        return token;
    }

    if (is_digit(c)) {
        // Handle numbers
        while (is_digit(*lexer->current) || *lexer->current == '.') {
            lexer->current++;
            lexer->column++;
        }
        token.length = lexer->current - token.start;
        token.kind = TOK_INTEGER;
        return token;
    }

    // Handle single-character tokens
    switch (c) {
        case '{': token.kind = TOK_LBRACE; break;
        case '}': token.kind = TOK_RBRACE; break;
        case ':': token.kind = TOK_COLON; break;
        case ';': token.kind = TOK_SEMICOLON; break;
        case '.': token.kind = TOK_DOT; break;
        default:
            token.kind = TOK_ERROR;
            lexer->error.code = ERROR_LEXER_INVALID_CHAR;
            lexer->error.line = token.line;
            lexer->error.column = token.column;
    }

    token.length = 1;
    return token;
}
