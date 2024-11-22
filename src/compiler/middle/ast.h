#ifndef CANT_AST_H
#define CANT_AST_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    AST_ECU_DEF,
    AST_SIGNAL_DEF,
    AST_PROCESS_DEF,
    AST_IDENTIFIER,
    AST_NUMBER,
    AST_FREQUENCY,
    AST_MEMORY_SPEC,
    AST_RANGE_SPEC,
    AST_FILTER_SPEC
} ASTNodeType;

typedef struct ASTNode ASTNode;
typedef struct {
    const char* name;
    uint32_t scope_level;
    ASTNode* declaration;
} Symbol;

typedef struct {
    Symbol** symbols;
    size_t capacity;
    size_t count;
    uint32_t scope_level;
} SymbolTable;

struct ASTNode {
    ASTNodeType type;
    uint32_t line;
    uint32_t column;
    
    union {
        // ECU Definition
        struct {
            ASTNode* identifier;
            ASTNode** declarations;
            size_t declaration_count;
        } ecu_def;
        
        // Signal Definition
        struct {
            ASTNode* identifier;
            ASTNode* protocol;
            ASTNode** properties;
            size_t property_count;
        } signal_def;
        
        // Process Definition
        struct {
            ASTNode* identifier;
            ASTNode* input;
            ASTNode* filter;
            ASTNode* output;
        } process_def;
        
        // Identifier
        struct {
            const char* name;
            Symbol* symbol;
        } identifier;
        
        // Number
        struct {
            union {
                int64_t i_val;
                double f_val;
            };
            bool is_float;
        } number;
        
        // Frequency Specification
        struct {
            int64_t value;
            const char* unit; // Hz, kHz, MHz
        } frequency;
        
        // Memory Specification
        struct {
            int64_t size;
            const char* unit; // B, KB, MB
        } memory;
    } as;
};

// AST Creation Functions
ASTNode* ast_create_ecu_def(ASTNode* identifier);
ASTNode* ast_create_signal_def(ASTNode* identifier, ASTNode* protocol);
ASTNode* ast_create_process_def(ASTNode* identifier);
ASTNode* ast_create_identifier(const char* name);
ASTNode* ast_create_number(int64_t value);
ASTNode* ast_create_float_number(double value);

// Symbol Table Functions
SymbolTable* symtable_create(void);
void symtable_destroy(SymbolTable* table);
bool symtable_insert(SymbolTable* table, Symbol* symbol);
Symbol* symtable_lookup(SymbolTable* table, const char* name);
void symtable_enter_scope(SymbolTable* table);
void symtable_exit_scope(SymbolTable* table);

// Memory Management
void ast_destroy(ASTNode* node);

#endif // CANT_AST_H
