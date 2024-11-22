#ifndef CANT_SEMANTIC_H
#define CANT_SEMANTIC_H

#include "../middle/ast.h"
#include "../middle/types.h"

typedef struct {
    const char* message;
    uint32_t line;
    uint32_t column;
    const char* file;
} Diagnostic;

typedef struct {
    Diagnostic* diagnostics;
    size_t capacity;
    size_t count;
} DiagnosticBag;

typedef struct Analyzer Analyzer;

Analyzer* analyzer_create(void);
void analyzer_destroy(Analyzer* analyzer);
bool analyzer_check(Analyzer* analyzer, ASTNode* ast);
DiagnosticBag* analyzer_get_diagnostics(Analyzer* analyzer);

#endif // CANT_SEMANTIC_H 