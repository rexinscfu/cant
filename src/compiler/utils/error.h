#ifndef CANT_ERROR_H
#define CANT_ERROR_H

#include <stdint.h>

// Error codes
typedef enum {
    ERROR_NONE = 0,
    ERROR_MEMORY,
    ERROR_IO,
    ERROR_SYNTAX,
    ERROR_SEMANTIC,
    ERROR_TYPE,
    ERROR_LINK,
    ERROR_RUNTIME,
    ERROR_LEXER_INVALID_CHAR,
    ERROR_PARSER_UNEXPECTED_TOKEN,
    ERROR_PATTERN_INVALID,
    ERROR_SIMD_NOT_SUPPORTED,
    ERROR_BUFFER_OVERFLOW,
    ERROR_INVALID_CONFIG
} ErrorCode;

// Error context
typedef struct {
    ErrorCode code;
    const char* message;
    const char* file;
    const char* function;
    uint32_t line;
    uint32_t column;
} Error;

// Error handling API
void set_error(ErrorCode code, const char* message, const char* function, uint32_t line);
void clear_error(void);
Error get_last_error(void);
const char* error_code_str(ErrorCode code);
bool has_error(void);

#endif // CANT_ERROR_H 