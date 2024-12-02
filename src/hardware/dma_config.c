#include "stm32f4xx_hal.h"
#include "../runtime/network/buffer_manager.h"

static DMA_HandleTypeDef hdma_rx;
static uint8_t dma_buffer[512] __attribute__((aligned(32)));

bool DMA_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    hdma_rx.Instance = DMA1_Stream0;
    hdma_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_rx.Init.Mode = DMA_CIRCULAR;
    hdma_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    
    if(HAL_DMA_Init(&hdma_rx) != HAL_OK) {
        return false;
    }
    
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    
    return true;
}

void DMA_StartReceive(void) {
    HAL_DMA_Start_IT(&hdma_rx, (uint32_t)&USART1->DR, 
                     (uint32_t)dma_buffer, sizeof(dma_buffer));
}

uint32_t DMA_GetPosition(void) {
    return sizeof(dma_buffer) - 
           __HAL_DMA_GET_COUNTER(&hdma_rx);
}

static uint32_t overflow_count = 0; 