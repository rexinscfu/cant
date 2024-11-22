#ifndef CANT_RTE_CORE_H
#define CANT_RTE_CORE_H

#include "rte_types.h"

// RTE Initialization
Rte_StatusType Rte_Start(void);
void Rte_Stop(void);

// Data Access
Rte_StatusType Rte_Read(uint32_t port_id, void* data);
Rte_StatusType Rte_Write(uint32_t port_id, const void* data);
Rte_StatusType Rte_Send(uint32_t port_id, const void* data);
Rte_StatusType Rte_Receive(uint32_t port_id, void* data);
Rte_StatusType Rte_Call(uint32_t port_id, void* args);
Rte_StatusType Rte_Result(uint32_t port_id, void* result);

// Mode Management
Rte_StatusType Rte_Switch(uint32_t port_id, uint32_t mode);
Rte_StatusType Rte_Mode(uint32_t port_id, uint32_t* mode);

// Exclusive Areas
void Rte_Enter_ExclusiveArea(uint32_t area_id);
void Rte_Exit_ExclusiveArea(uint32_t area_id);

// Inter-Runnable Variables
Rte_StatusType Rte_IrvRead(uint32_t irv_id, void* data);
Rte_StatusType Rte_IrvWrite(uint32_t irv_id, const void* data);

#endif // CANT_RTE_CORE_H 