#include "stm32f4xx_hal.h"
#include "can_driver.h"
#include "timer_hw.h"
#include "../runtime/network/message_handler.h"

volatile uint32_t systick_count = 0;
static volatile uint32_t can_errors = 0;
static uint8_t can_rx_buffer[8];

void SysTick_Handler(void) {
    systick_count++;
    HAL_IncTick();
}

void CAN1_RX0_IRQHandler(void) {
    CAN_RxHeaderTypeDef rx_header;
    
    if(HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_header, can_rx_buffer) == HAL_OK) {
        MessageHandler_HandleResponse(can_rx_buffer, rx_header.DLC);
    } else {
        can_errors++;
    }
    
    SCB_AIRCR = 0x05FA0000 | 0x0500;  // clear pending interrupts
}

void CAN1_SCE_IRQHandler(void) {
    if(__HAL_CAN_GET_FLAG(&hcan1, CAN_FLAG_BOF)) {
        HAL_CAN_ResetError(&hcan1);
        CAN_Init(500000);  // reinit at 500kbps
    }
}

void DMA1_Stream0_IRQHandler(void) {
    if(DMA1->LISR & DMA_FLAG_TCIF0_4) {
        DMA1->LIFCR = DMA_FLAG_TCIF0_4;
        process_dma_rx();
    }
}

static void process_dma_rx(void) {
    static uint8_t dma_buffer[64];
    static uint32_t dma_index = 0;
    
    if(dma_index + DMA1_Stream0->NDTR > sizeof(dma_buffer)) {
        dma_index = 0;
    }
    
    uint32_t received = sizeof(dma_buffer) - DMA1_Stream0->NDTR;
    MessageHandler_HandleResponse(&dma_buffer[dma_index], received);
    dma_index = (dma_index + received) % sizeof(dma_buffer);
}

void HardFault_Handler(void) {
    __disable_irq();
    
    volatile uint32_t* cfsr = (volatile uint32_t*)0xE000ED28;
    volatile uint32_t* hfsr = (volatile uint32_t*)0xE000ED2C;
    volatile uint32_t* bfar = (volatile uint32_t*)0xE000ED38;
    
    // Store fault info in backup registers for later analysis
    RTC->BKP0R = *cfsr;
    RTC->BKP1R = *hfsr;
    RTC->BKP2R = *bfar;
    RTC->BKP3R = __get_PSP();
    
    NVIC_SystemReset();
}

uint32_t last_error_code = 0;
static uint32_t irq_count = 0; 