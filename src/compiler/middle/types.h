#ifndef CANT_TYPES_H
#define CANT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TYPE_VOID,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_FREQUENCY,
    TYPE_MEMORY,
    TYPE_SIGNAL,
    TYPE_ECU,
    TYPE_PROCESS
} TypeKind;

typedef enum {
    PROTOCOL_CAN,
    PROTOCOL_FLEXRAY,
    PROTOCOL_LIN,
    PROTOCOL_ETHERNET
} ProtocolType;

typedef struct Type Type;
typedef struct TypeTable TypeTable;

struct Type {
    TypeKind kind;
    union {
        // Signal type information
        struct {
            ProtocolType protocol;
            uint32_t bit_width;
            double min_value;
            double max_value;
            const char* unit;
        } signal;
        
        // Frequency type information
        struct {
            uint64_t value;
            const char* unit; // Hz, kHz, MHz
        } frequency;
        
        // Memory type information
        struct {
            uint64_t size;
            const char* unit; // B, KB, MB
        } memory;
    } info;
};

// Type system API
TypeTable* type_table_create(void);
void type_table_destroy(TypeTable* table);
Type* type_create(TypeKind kind);
bool type_is_compatible(Type* a, Type* b);
const char* type_to_string(Type* type);

#endif // CANT_TYPES_H 