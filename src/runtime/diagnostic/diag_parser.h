#ifndef CANT_DIAG_PARSER_H
#define DIAG_PARSER_H

#include "diag_core.h"

// Parser result codes
typedef enum {
    PARSER_RESULT_OK,
    PARSER_RESULT_INVALID_FORMAT,
    PARSER_RESULT_INVALID_LENGTH,
    PARSER_RESULT_INVALID_SERVICE,
    PARSER_RESULT_INVALID_SUBFUNC,
    PARSER_RESULT_ERROR
} DiagParserResult;

// Parser functions
DiagParserResult DiagParser_ParseRequest(const uint8_t* data, uint32_t length, DiagMessage* message);
DiagParserResult DiagParser_ParseResponse(const uint8_t* data, uint32_t length, DiagResponse* response);

bool DiagParser_FormatRequest(const DiagMessage* message, uint8_t* buffer, uint32_t* length);
bool DiagParser_FormatResponse(const DiagResponse* response, uint8_t* buffer, uint32_t* length);

const char* DiagParser_GetResultString(DiagParserResult result);

#endif // CANT_DIAG_PARSER_H 