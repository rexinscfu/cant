#ifndef DMA_CONFIG_H
#define DMA_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

bool DMA_Init(void);
void DMA_StartReceive(void);
uint32_t DMA_GetPosition(void);

#endif 