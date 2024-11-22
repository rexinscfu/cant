#include "ast.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_SYMBOL_CAPACITY 64

ASTNode* ast_create_node(ASTNodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (node) {
        node->type = type;
    }
    return node;
}

ASTNode* ast_create_ecu_def(ASTNode* identifier) {
    ASTNode* node = ast_create_node(AST_ECU_DEF);
    if (node) {
        node->as.ecu_def.identifier = identifier;
        node->as.ecu_def.declarations = NULL;
        node->as.ecu_def.declaration_count = 0;
    }
    return node;
}

ASTNode* ast_create_signal_def(ASTNode* identifier, ASTNode* protocol) {
    ASTNode* node = ast_create_node(AST_SIGNAL_DEF);
    if (node) {
        node->as.signal_def.identifier = identifier;
        node->as.signal_def.protocol = protocol;
        node->as.signal_def.properties = NULL;
        node->as.signal_def.property_count = 0;
    }
    return node;
}

ASTNode* ast_create_identifier(const char* name) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER);
    if (node) {
        node->as.identifier.name = strdup(name);
        node->as.identifier.symbol = NULL;
    }
    return node;
}

void ast_destroy(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_ECU_DEF:
            ast_destroy(node->as.ecu_def.identifier);
            for (size_t i = 0; i < node->as.ecu_def.declaration_count; i++) {
                ast_destroy(node->as.ecu_def.declarations[i]);
            }
            free(node->as.ecu_def.declarations);
            break;
            
        case AST_SIGNAL_DEF:
            ast_destroy(node->as.signal_def.identifier);
            ast_destroy(node->as.signal_def.protocol);
            for (size_t i = 0; i < node->as.signal_def.property_count; i++) {
                ast_destroy(node->as.signal_def.properties[i]);
            }
            free(node->as.signal_def.properties);
            break;
            
        case AST_IDENTIFIER:
            free((void*)node->as.identifier.name);
            break;
            
        default:
            break;
    }
    
    free(node);
}

SymbolTable* symtable_create(void) {
    SymbolTable* table = malloc(sizeof(SymbolTable));
    if (table) {
        table->symbols = calloc(INITIAL_SYMBOL_CAPACITY, sizeof(Symbol*));
        table->capacity = INITIAL_SYMBOL_CAPACITY;
        table->count = 0;
        table->scope_level = 0;
    }
    return table;
}

void symtable_destroy(SymbolTable* table) {
    if (!table) return;
    
    for (size_t i = 0; i < table->count; i++) {
        free(table->symbols[i]);
    }
    free(table->symbols);
    free(table);
}

bool symtable_insert(SymbolTable* table, Symbol* symbol) {
    if (table->count >= table->capacity) {
        size_t new_capacity = table->capacity * 2;
        Symbol** new_symbols = realloc(table->symbols, 
                                     new_capacity * sizeof(Symbol*));
        if (!new_symbols) return false;
        
        table->symbols = new_symbols;
        table->capacity = new_capacity;
    }
    
    symbol->scope_level = table->scope_level;
    table->symbols[table->count++] = symbol;
    return true;
}

Symbol* symtable_lookup(SymbolTable* table, const char* name) {
    for (size_t i = table->count; i > 0; i--) {
        if (strcmp(table->symbols[i-1]->name, name) == 0) {
            return table->symbols[i-1];
        }
    }
    return NULL;
}
