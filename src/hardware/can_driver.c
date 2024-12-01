#include "can_driver.h"
#include "stm32f4xx_hal.h"

static CAN_HandleTypeDef hcan1;
static CANRxCallback rx_callback;
static CANStats can_stats;

uint32_t tx_mailbox = CAN_TX_MAILBOX0;
static uint8_t rx_data[8];
bool init_done = false;

bool CAN_Init(uint32_t baudrate) {
    GPIO_InitTypeDef GPIO_InitStruct;
    
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 6;
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_4TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = ENABLE;
    hcan1.Init.AutoWakeUp = DISABLE;
    hcan1.Init.AutoRetransmission = ENABLE;
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;
    
    if (HAL_CAN_Init(&hcan1) != HAL_OK) {
        return false;
    }
    
    CAN_FilterTypeDef filter;
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    
    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK) {
        return false;
    }
    
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    
    init_done = true;
    return true;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_header;
    
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
        can_stats.rx_count++;
        if (rx_callback) {
            rx_callback(rx_header.StdId, rx_data, rx_header.DLC);
        }
    }
}

bool CAN_Transmit(uint32_t id, uint8_t* data, uint8_t len) {
    if (!init_done || !data || len > 8) return false;
    
    CAN_TxHeaderTypeDef tx_header;
    tx_header.StdId = id;
    tx_header.ExtId = 0;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.IDE = CAN_ID_STD;
    tx_header.DLC = len;
    tx_header.TransmitGlobalTime = DISABLE;
    
    if (HAL_CAN_AddTxMessage(&hcan1, &tx_header, data, &tx_mailbox) == HAL_OK) {
        can_stats.tx_count++;
        return true;
    }
    
    can_stats.error_count++;
    can_stats.last_error = HAL_CAN_GetError(&hcan1);
    return false;
}

void CAN_RegisterRxCallback(CANRxCallback callback) {
    rx_callback = callback;
}

uint32_t CAN_GetErrorStatus(void) {
    return HAL_CAN_GetError(&hcan1);
}

void CAN_GetStats(CANStats* stats) {
    if (stats) {
        memcpy(stats, &can_stats, sizeof(CANStats));
    }
}

void CAN_ResetStats(void) {
    memset(&can_stats, 0, sizeof(CANStats));
}

void CAN_Deinit(void) {
    HAL_CAN_Stop(&hcan1);
    HAL_CAN_DeInit(&hcan1);
    init_done = false;
} 