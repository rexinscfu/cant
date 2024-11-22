#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct TypeTable {
    Type** types;
    size_t capacity;
    size_t count;
};

TypeTable* type_table_create(void) {
    TypeTable* table = malloc(sizeof(TypeTable));
    if (table) {
        table->types = calloc(64, sizeof(Type*));
        table->capacity = 64;
        table->count = 0;
    }
    return table;
}

void type_table_destroy(TypeTable* table) {
    if (!table) return;
    for (size_t i = 0; i < table->count; i++) {
        free(table->types[i]);
    }
    free(table->types);
    free(table);
}

Type* type_create(TypeKind kind) {
    Type* type = calloc(1, sizeof(Type));
    if (type) {
        type->kind = kind;
    }
    return type;
}

bool type_is_compatible(Type* a, Type* b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_SIGNAL:
            return a->info.signal.protocol == b->info.signal.protocol &&
                   a->info.signal.bit_width == b->info.signal.bit_width;
        case TYPE_FREQUENCY:
            // Convert to common unit (Hz) before comparing
            return true; // TODO: Implement conversion
        case TYPE_MEMORY:
            // Convert to common unit (bytes) before comparing
            return true; // TODO: Implement conversion
        default:
            return true;
    }
}

const char* type_to_string(Type* type) {
    static char buffer[128];
    if (!type) return "invalid";
    
    switch (type->kind) {
        case TYPE_VOID: return "void";
        case TYPE_INTEGER: return "integer";
        case TYPE_FLOAT: return "float";
        case TYPE_ECU: return "ecu";
        case TYPE_PROCESS: return "process";
        case TYPE_SIGNAL:
            snprintf(buffer, sizeof(buffer), "signal<%s,%d>",
                    type->info.signal.protocol == PROTOCOL_CAN ? "CAN" :
                    type->info.signal.protocol == PROTOCOL_FLEXRAY ? "FlexRay" :
                    type->info.signal.protocol == PROTOCOL_LIN ? "LIN" : "Ethernet",
                    type->info.signal.bit_width);
            return buffer;
        default:
            return "unknown";
    }
} 