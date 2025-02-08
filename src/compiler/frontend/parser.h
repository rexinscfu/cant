#ifndef CANT_PARSER_H
#define CANT_PARSER_H

#include "lexer.h"
#include <stdint.h>

typedef enum {
    NODE_PROGRAM,
    NODE_ECU_DECL,
    NODE_SIGNAL_DECL,
    NODE_CAN_CONFIG,
    NODE_PROCESS_DECL,
    
    // Diagnostic nodes
    NODE_DIAG_SERVICE,
    NODE_DIAG_SESSION,
    NODE_DIAG_SECURITY,
    NODE_DIAG_REQUEST,
    NODE_DIAG_RESPONSE,
    NODE_DIAG_ROUTINE,
    NODE_DIAG_DID,
    NODE_DIAG_PATTERN,
    NODE_DIAG_FLOW,
    
    // CAN Frame nodes
    NODE_FRAME_DECL,
    NODE_FRAME_ID,
    NODE_FRAME_DLC,
    NODE_FRAME_DATA,
    NODE_FRAME_PATTERN,
    
    // Expression nodes
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_INTEGER_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_HEX_LITERAL,
    NODE_BINARY_LITERAL,
    NODE_STRING_LITERAL,
    NODE_IDENTIFIER,
    
    // Statement nodes
    NODE_BLOCK,
    NODE_IF_STMT,
    NODE_MATCH_STMT,
    NODE_TIMEOUT_STMT,
    NODE_ASSIGNMENT
} NodeKind;

typedef struct Node Node;
typedef struct NodeList NodeList;

struct NodeList {
    Node* node;
    NodeList* next;
};

typedef struct {
    uint32_t id;
    uint8_t dlc;
    bool extended;
    bool periodic;
    uint32_t period_ms;
} FrameConfig;

typedef struct {
    uint8_t level;
    uint32_t timeout_ms;
    NodeList* patterns;
} DiagConfig;

struct Node {
    NodeKind kind;
    Token token;
    union {
        struct {
            NodeList* declarations;
        } program;
        
        struct {
            char* name;
            NodeList* configs;
        } ecu_decl;
        
        struct {
            char* name;
            uint32_t size;
            char* unit;
        } signal_decl;
        
        struct {
            uint32_t baudrate;
            NodeList* frames;
        } can_config;
        
        struct {
            char* name;
            NodeList* statements;
        } process_decl;
        
        struct {
            uint16_t id;
            DiagConfig config;
            NodeList* handlers;
        } diag_service;
        
        struct {
            uint8_t level;
            uint32_t timeout;
            NodeList* transitions;
        } diag_session;
        
        struct {
            uint8_t level;
            NodeList* access_rules;
        } diag_security;
        
        struct {
            uint32_t id;
            NodeList* data;
            uint32_t timeout;
        } diag_request;
        
        struct {
            uint32_t id;
            NodeList* patterns;
            NodeList* handlers;
        } diag_response;
        
        struct {
            uint16_t id;
            char* name;
            NodeList* params;
        } diag_routine;
        
        struct {
            uint16_t id;
            uint32_t size;
            char* format;
        } diag_did;
        
        struct {
            NodeList* conditions;
            Node* handler;
        } diag_pattern;
        
        struct {
            uint32_t control;
            uint32_t timeout;
        } diag_flow;
        
        struct {
            FrameConfig config;
            NodeList* data;
        } frame_decl;
        
        struct {
            uint32_t value;
            bool is_mask;
        } frame_id;
        
        struct {
            uint8_t value;
            uint8_t mask;
        } frame_dlc;
        
        struct {
            uint8_t* data;
            uint8_t* mask;
            uint8_t length;
        } frame_data;
        
        struct {
            NodeList* conditions;
            Node* handler;
        } frame_pattern;
        
        struct {
            Node* left;
            Token op;
            Node* right;
        } binary_expr;
        
        struct {
            Token op;
            Node* operand;
        } unary_expr;
        
        uint64_t int_value;
        double float_value;
        char* string_value;
        char* identifier;
        
        struct {
            NodeList* statements;
        } block;
        
        struct {
            Node* condition;
            Node* then_branch;
            Node* else_branch;
        } if_stmt;
        
        struct {
            Node* value;
            NodeList* cases;
        } match_stmt;
        
        struct {
            uint32_t duration;
            Node* handler;
        } timeout_stmt;
        
        struct {
            Node* target;
            Node* value;
        } assignment;
    } as;
};

typedef struct Parser Parser;

// Parser API
Parser* parser_create(Lexer* lexer);
void parser_destroy(Parser* parser);
Node* parser_parse(Parser* parser);
void node_destroy(Node* node);

// Error handling
typedef struct {
    const char* message;
    uint32_t line;
    uint32_t column;
} ParseError;

ParseError parser_get_error(const Parser* parser);

#endif // CANT_PARSER_H
