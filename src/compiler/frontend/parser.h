#ifndef CANT_PARSER_H
#define CANT_PARSER_H

#include "lexer.h"
#include "../middle/ast.h"

typedef struct Parser Parser;

Parser* parser_create(const char* source);
void parser_destroy(Parser* parser);
ASTNode* parser_parse(Parser* parser);

// Error handling
typedef struct {
    const char* message;
    uint32_t line;
    uint32_t column;
} ParseError;

ParseError parser_get_error(const Parser* parser);

#endif // CANT_PARSER_H
