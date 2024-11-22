#include "error.h"
#include <stdio.h>

static const char* error_messages[] = {
    [ERROR_NONE] = "No error",
    [ERROR_LEXER_INVALID_CHAR] = "Invalid character encountered",
    [ERROR_LEXER_INVALID_NUMBER] = "Invalid number format",
    [ERROR_LEXER_UNTERMINATED_STRING] = "Unterminated string literal",
    [ERROR_MEMORY] = "Memory allocation failed"
};

void error_init(void) {
    // Initialize error handling system
}

void error_report(Error error) {
    fprintf(stderr, "%s:%u:%u: error: %s\n",
            error.file,
            error.line,
            error.column,
            error.message ? error.message : error_get_message(error.code));
}

const char* error_get_message(ErrorCode code) {
    return error_messages[code];
}
