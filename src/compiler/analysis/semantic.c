#include "semantic.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_DIAGNOSTIC_CAPACITY 32

struct Analyzer {
    TypeTable* types;
    SymbolTable* symbols;
    DiagnosticBag diagnostics;
    const char* current_file;
};

static void emit_diagnostic(Analyzer* analyzer, const char* message,
                          uint32_t line, uint32_t column) {
    DiagnosticBag* bag = &analyzer->diagnostics;
    
    if (bag->count >= bag->capacity) {
        size_t new_capacity = bag->capacity * 2;
        Diagnostic* new_diagnostics = realloc(bag->diagnostics,
                                            new_capacity * sizeof(Diagnostic));
        if (!new_diagnostics) return;
        
        bag->diagnostics = new_diagnostics;
        bag->capacity = new_capacity;
    }
    
    Diagnostic* diag = &bag->diagnostics[bag->count++];
    diag->message = strdup(message);
    diag->line = line;
    diag->column = column;
    diag->file = analyzer->current_file;
}

static bool check_ecu_definition(Analyzer* analyzer, ASTNode* node) {
    if (!node || node->type != AST_ECU_DEF) return false;
    
    // Check for duplicate ECU names
    Symbol* existing = symtable_lookup(analyzer->symbols,
                                     node->as.ecu_def.identifier->as.identifier.name);
    if (existing) {
        emit_diagnostic(analyzer, "Duplicate ECU definition",
                       node->line, node->column);
        return false;
    }
    
    // Create and register ECU type
    Type* ecu_type = type_create(TYPE_ECU);
    if (!ecu_type) return false;
    
    // Create and register symbol
    Symbol* symbol = malloc(sizeof(Symbol));
    if (!symbol) {
        free(ecu_type);
        return false;
    }
    
    symbol->name = strdup(node->as.ecu_def.identifier->as.identifier.name);
    symbol->declaration = node;
    
    if (!symtable_insert(analyzer->symbols, symbol)) {
        free(ecu_type);
        free(symbol);
        return false;
    }
    
    // Check ECU declarations
    for (size_t i = 0; i < node->as.ecu_def.declaration_count; i++) {
        ASTNode* decl = node->as.ecu_def.declarations[i];
        switch (decl->type) {
            case AST_SIGNAL_DEF:
                if (!check_signal_definition(analyzer, decl)) {
                    return false;
                }
                break;
            // Add other declaration types here
            default:
                emit_diagnostic(analyzer, "Invalid declaration in ECU",
                              decl->line, decl->column);
                return false;
        }
    }
    
    return true;
}

static bool check_signal_definition(Analyzer* analyzer, ASTNode* node) {
    if (!node || node->type != AST_SIGNAL_DEF) return false;
    
    // Check for duplicate signal names
    Symbol* existing = symtable_lookup(analyzer->symbols,
                                     node->as.signal_def.identifier->as.identifier.name);
    if (existing) {
        emit_diagnostic(analyzer, "Duplicate signal definition",
                       node->line, node->column);
        return false;
    }
    
    // Validate signal properties
    Type* signal_type = type_create(TYPE_SIGNAL);
    if (!signal_type) return false;
    
    // Check protocol
    switch (node->as.signal_def.protocol->type) {
        case AST_IDENTIFIER:
            if (strcmp(node->as.signal_def.protocol->as.identifier.name, "CAN") == 0) {
                signal_type->info.signal.protocol = PROTOCOL_CAN;
            } else if (strcmp(node->as.signal_def.protocol->as.identifier.name, "FlexRay") == 0) {
                signal_type->info.signal.protocol = PROTOCOL_FLEXRAY;
            } else {
                emit_diagnostic(analyzer, "Invalid protocol type",
                              node->line, node->column);
                free(signal_type);
                return false;
            }
            break;
        default:
            emit_diagnostic(analyzer, "Expected protocol identifier",
                          node->line, node->column);
            free(signal_type);
            return false;
    }
    
    // Validate signal properties
    for (size_t i = 0; i < node->as.signal_def.property_count; i++) {
        ASTNode* prop = node->as.signal_def.properties[i];
        // TODO: Implement property validation
    }
    
    return true;
}

Analyzer* analyzer_create(void) {
    Analyzer* analyzer = malloc(sizeof(Analyzer));
    if (!analyzer) return NULL;
    
    analyzer->types = type_table_create();
    analyzer->symbols = symtable_create();
    analyzer->diagnostics.diagnostics = calloc(INITIAL_DIAGNOSTIC_CAPACITY,
                                             sizeof(Diagnostic));
    analyzer->diagnostics.capacity = INITIAL_DIAGNOSTIC_CAPACITY;
    analyzer->diagnostics.count = 0;
    analyzer->current_file = NULL;
    
    return analyzer;
}

void analyzer_destroy(Analyzer* analyzer) {
    if (!analyzer) return;
    
    type_table_destroy(analyzer->types);
    symtable_destroy(analyzer->symbols);
    
    for (size_t i = 0; i < analyzer->diagnostics.count; i++) {
        free((void*)analyzer->diagnostics.diagnostics[i].message);
    }
    free(analyzer->diagnostics.diagnostics);
    
    free(analyzer);
}

bool analyzer_check(Analyzer* analyzer, ASTNode* ast) {
    if (!ast) return false;
    
    switch (ast->type) {
        case AST_ECU_DEF:
            return check_ecu_definition(analyzer, ast);
        default:
            emit_diagnostic(analyzer, "Unexpected top-level declaration",
                          ast->line, ast->column);
            return false;
    }
}

DiagnosticBag* analyzer_get_diagnostics(Analyzer* analyzer) {
    return &analyzer->diagnostics;
} 