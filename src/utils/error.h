#ifndef CANT_ERROR_H
#define CANT_ERROR_H

#include <stdint.h>

typedef enum {
    ERROR_NONE,
    ERROR_LEXER_INVALID_CHAR,
    ERROR_LEXER_INVALID_NUMBER,
    ERROR_LEXER_UNTERMINATED_STRING,
    ERROR_MEMORY
} ErrorCode;

typedef struct {
    ErrorCode code;
    const char* message;
    uint32_t line;
    uint32_t column;
    const char* file;
} Error;

void error_init(void);
void error_report(Error error);
const char* error_get_message(ErrorCode code);

#endif // CANT_ERROR_H
