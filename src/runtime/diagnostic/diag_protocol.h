#ifndef CANT_DIAG_PROTOCOL_H
#define DIAG_PROTOCOL_H

#include "diag_core.h"

// Protocol handlers
typedef struct {
    bool (*init)(void);
    void (*deinit)(void);
    bool (*send_message)(const DiagMessage* message);
    bool (*receive_message)(DiagMessage* message);
    bool (*start_session)(DiagSessionType session);
    bool (*end_session)(void);
    bool (*security_access)(DiagSecurityLevel level, const uint8_t* key, uint32_t length);
    bool (*tester_present)(void);
    void (*handle_timeout)(void);
} DiagProtocolHandler;

// Protocol-specific functions
bool DiagProtocol_Init(NetProtocolType protocol);
void DiagProtocol_Deinit(void);

bool DiagProtocol_SendMessage(const DiagMessage* message);
bool DiagProtocol_ReceiveMessage(DiagMessage* message);

bool DiagProtocol_StartSession(DiagSessionType session);
bool DiagProtocol_EndSession(void);

bool DiagProtocol_SecurityAccess(DiagSecurityLevel level, const uint8_t* key, uint32_t length);
bool DiagProtocol_TesterPresent(void);

void DiagProtocol_HandleTimeout(void);
bool DiagProtocol_IsActive(void);

#endif // CANT_DIAG_PROTOCOL_H 