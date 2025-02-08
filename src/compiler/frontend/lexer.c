#include "lexer.h"
#include "../utils/error.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_IDENTIFIER_LENGTH 256
#define MAX_STRING_LENGTH 1024

struct Lexer {
    const char* source;
    const char* current;
    uint32_t line;
    uint32_t column;
    char string_buffer[MAX_STRING_LENGTH];
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
    {"diagnostic", TOK_DIAGNOSTIC},
    {"service", TOK_SERVICE},
    {"session", TOK_SESSION},
    {"security", TOK_SECURITY},
    {"request", TOK_REQUEST},
    {"response", TOK_RESPONSE},
    {"routine", TOK_ROUTINE},
    {"did", TOK_DID},
    {"pattern", TOK_PATTERN},
    {"match", TOK_MATCH},
    {"timeout", TOK_TIMEOUT},
    {"flow", TOK_FLOW},
    {"frame", TOK_FRAME},
    {"id", TOK_ID},
    {"dlc", TOK_DLC},
    {"extended", TOK_EXTENDED},
    {"periodic", TOK_PERIODIC},
    {"trigger", TOK_TRIGGER},
    {NULL, TOK_EOF}
};

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_alnum(char c) {
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
        while (is_alnum(*lexer->current)) {
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

const char* lexer_token_kind_name(TokenKind kind) {
    static const char* names[] = {
        "EOF",
        "ECU", "SIGNAL", "CAN", "FLEXRAY", "PROCESS", "INPUT", "OUTPUT", "FILTER",
        "DIAGNOSTIC", "SERVICE", "SESSION", "SECURITY", "REQUEST", "RESPONSE",
        "ROUTINE", "DID", "PATTERN", "MATCH", "TIMEOUT", "FLOW",
        "FRAME", "ID", "DLC", "EXTENDED", "PERIODIC", "TRIGGER",
        "INTEGER", "FLOAT", "IDENTIFIER", "HEX", "BINARY", "STRING",
        "ASSIGN", "PLUS", "MINUS", "STAR", "SLASH", "PERCENT",
        "AMPERSAND", "PIPE", "CARET", "TILDE", "SHIFT_LEFT", "SHIFT_RIGHT",
        "EQUAL", "NOT_EQUAL", "LESS", "LESS_EQUAL", "GREATER", "GREATER_EQUAL",
        "LBRACE", "RBRACE", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET",
        "COLON", "SEMICOLON", "DOT", "COMMA", "ARROW",
        "ERROR", "COMMENT"
    };
    return names[kind];
}

bool lexer_is_keyword(const char* identifier, size_t length) {
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strlen(keywords[i].keyword) == length &&
            memcmp(identifier, keywords[i].keyword, length) == 0) {
            return true;
        }
    }
    return false;
}
