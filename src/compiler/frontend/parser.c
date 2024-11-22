#include "parser.h"
#include <stdlib.h>
#include <string.h>

struct Parser {
    Lexer* lexer;
    Token current;
    Token previous;
    bool had_error;
    ParseError error;
    SymbolTable* symbols;
};

static void parser_advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = lexer_next_token(parser->lexer);
}

static bool parser_check(Parser* parser, TokenKind kind) {
    return parser->current.kind == kind;
}

static bool parser_match(Parser* parser, TokenKind kind) {
    if (parser_check(parser, kind)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static void parser_error(Parser* parser, const char* message) {
    if (parser->had_error) return;
    
    parser->had_error = true;
    parser->error.message = message;
    parser->error.line = parser->current.line;
    parser->error.column = parser->current.column;
}

static void parser_consume(Parser* parser, TokenKind kind, const char* message) {
    if (parser->current.kind == kind) {
        parser_advance(parser);
    } else {
        parser_error(parser, message);
    }
}

static ASTNode* parse_ecu_definition(Parser* parser) {
    // Expect: ecu identifier { ... }
    parser_consume(parser, TOK_ECU, "Expected 'ecu' keyword");
    
    if (!parser_match(parser, TOK_IDENTIFIER)) {
        parser_error(parser, "Expected ECU identifier");
        return NULL;
    }
    
    ASTNode* identifier = ast_create_identifier(
        strndup(parser->previous.start, parser->previous.length)
    );
    
    ASTNode* ecu = ast_create_ecu_def(identifier);
    
    parser_consume(parser, TOK_LBRACE, "Expected '{' after ECU identifier");
    
    // Parse ECU body
    while (!parser_check(parser, TOK_RBRACE) && 
           !parser_check(parser, TOK_EOF)) {
        // Parse declarations
        // TODO: Implement declaration parsing
        parser_advance(parser);
    }
    
    parser_consume(parser, TOK_RBRACE, "Expected '}' after ECU body");
    
    return ecu;
}

Parser* parser_create(const char* source) {
    Parser* parser = malloc(sizeof(Parser));
    if (!parser) return NULL;
    
    parser->lexer = lexer_create(source);
    if (!parser->lexer) {
        free(parser);
        return NULL;
    }
    
    parser->symbols = symtable_create();
    if (!parser->symbols) {
        lexer_destroy(parser->lexer);
        free(parser);
        return NULL;
    }
    
    parser->had_error = false;
    parser_advance(parser);
    return parser;
}

void parser_destroy(Parser* parser) {
    if (!parser) return;
    lexer_destroy(parser->lexer);
    symtable_destroy(parser->symbols);
    free(parser);
}

ASTNode* parser_parse(Parser* parser) {
    // For now, we only parse ECU definitions
    return parse_ecu_definition(parser);
}

ParseError parser_get_error(const Parser* parser) {
    return parser->error;
}
