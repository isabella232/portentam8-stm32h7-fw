/*
 * Firmware for the Portenta X8 STM32H747AIIX/Cortex-M7 core.
 * Copyright (C) 2022 Arduino (http://www.arduino.cc/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**************************************************************************************
 * INCLUDE
 **************************************************************************************/

#include "uart.h"
#include "system.h"
#include "peripherals.h"
#include "main.h"
#include "ringbuffer.h"
#include "rpc.h"

/**************************************************************************************
 * TYPEDEF
 **************************************************************************************/

struct __attribute__((packed, aligned(4))) uartPacket {
  uint8_t bits: 4;
  uint8_t stop_bits: 2;
  uint8_t parity: 2;
  uint8_t flow_control: 1;
  uint32_t baud: 23;
};

/**************************************************************************************
 * GLOBAL VARIABLES
 **************************************************************************************/

UART_HandleTypeDef huart2;

ring_buffer_t uart_ring_buffer;
ring_buffer_t uart_tx_ring_buffer;
ring_buffer_t virtual_uart_ring_buffer;

static uint8_t uart_rxbuf[1024];

/**************************************************************************************
 * FUNCTION DEFINITION
 **************************************************************************************/

int _write(int file, char *ptr, int len)
{
  /* Try to directly transmit the data. If currently
   * a UART transmission is ongoing, this will return
   * HAL_BUSY.
   */
  if (HAL_OK == HAL_UART_Transmit_IT(&huart2, ptr, len))
    return len;

  /* Direct transmission did not work, let's store it
   * in the ringbuffer instead. Start by disabling
   * interrupts to avoid race conditions from accessing
   * uart_tx_ring_buffer from both IRQ and normal context.
   */
  __disable_irq();
  /* Enqueue data to write into ringbuffer. */
  ring_buffer_queue_arr(&uart_tx_ring_buffer, ptr, len);
  /* Reenable interrupts. */
  __enable_irq();
  return len;
}

int _read(int file, char *ptr, int len) {

}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (ring_buffer_is_empty(&uart_tx_ring_buffer))
    return;

  /* Dequeue the oldest data byte. */
  char data[16] = {0};
  uint16_t const bytes_read = ring_buffer_dequeue_arr(&uart_tx_ring_buffer, data, sizeof(data));
  /* Transmit data. */
  HAL_UART_Transmit_IT(&huart2, data, bytes_read);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  ring_buffer_queue_arr(&uart_ring_buffer, uart_rxbuf, Size);
  HAL_UARTEx_ReceiveToIdle_IT(&huart2, uart_rxbuf, sizeof(uart_rxbuf));
}

static void MX_USART2_UART_Init(void) {

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_2) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_2) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart2) != HAL_OK) {
    Error_Handler();
  }

/*
  UART_WakeUpTypeDef event = 
    { .WakeUpEvent = UART_WAKEUP_ON_STARTBIT };
  HAL_UARTEx_StopModeWakeUpSourceConfig(&huart2, event);
  HAL_UARTEx_EnableStopMode(&huart2);
*/
  UART2_enable_rx_irq();
}

/**
 * @brief UART MSP Initialization
 * This function configures the hardware resources used in this example
 * @param huart: UART handle pointer
 * @retval None
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if (huart->Instance == USART2) {
    /* USER CODE BEGIN USART2_MspInit 0 */

    /* USER CODE END USART2_MspInit 0 */
    /** Initializes the peripherals clock
     */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInitStruct.Usart234578ClockSelection =
        RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
      Error_Handler();
    }

    /* Peripheral clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PD3     ------> USART2_CTS
    PD6     ------> USART2_RX
    PD5     ------> USART2_TX
    PD4     ------> USART2_RTS
    */
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    /* USER CODE BEGIN USART2_MspInit 1 */

    /* USER CODE END USART2_MspInit 1 */
  }
}

/**
 * @brief UART MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param huart: UART handle pointer
 * @retval None
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    /* USER CODE BEGIN USART2_MspDeInit 0 */

    /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PD3     ------> USART2_CTS
    PD6     ------> USART2_RX
    PD5     ------> USART2_TX
    PD4     ------> USART2_RTS
    */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4);

    /* USART2 interrupt DeInit */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    /* USER CODE BEGIN USART2_MspDeInit 1 */

    /* USER CODE END USART2_MspDeInit 1 */
  }
}

void uart_handler(uint8_t opcode, uint8_t *data, uint16_t size) {
  if (opcode == CONFIGURE) {
    uart_configure(data);
  }
  if (opcode == DATA) {
    uart_write(data, size);
  }
}

void virtual_uart_handler(uint8_t opcode, uint8_t *data, uint16_t size) {
  serial_rpc_write(data, size);
}


void uart_init() {
  MX_USART2_UART_Init();

  ring_buffer_init(&uart_ring_buffer);
  ring_buffer_init(&uart_tx_ring_buffer);
  ring_buffer_init(&virtual_uart_ring_buffer);

  register_peripheral_callback(PERIPH_UART, &uart_handler);
  register_peripheral_callback(PERIPH_VIRTUAL_UART, &virtual_uart_handler);
}

int uart_write(uint8_t *data, uint16_t size) {
  return _write(0, data, size);
}

int uart_data_available() {
  return !ring_buffer_is_empty(&uart_ring_buffer);
}

void uart_handle_data() {
  uint8_t temp_buf[RING_BUFFER_SIZE];
  __disable_irq();
  int cnt = ring_buffer_dequeue_arr(&uart_ring_buffer, temp_buf, ring_buffer_num_items(&uart_ring_buffer));
  __enable_irq();
  enqueue_packet(PERIPH_UART, DATA, cnt, temp_buf);
}

int virtual_uart_data_available() {
  return !ring_buffer_is_empty(&virtual_uart_ring_buffer);
}

void virtual_uart_handle_data() {
  uint8_t temp_buf[RING_BUFFER_SIZE];
  __disable_irq();
  int cnt = ring_buffer_dequeue_arr(&virtual_uart_ring_buffer, temp_buf, ring_buffer_num_items(&virtual_uart_ring_buffer));
  __enable_irq();
  enqueue_packet(PERIPH_VIRTUAL_UART, DATA, cnt, temp_buf);
}

void UART2_enable_rx_irq() {
  //__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXFNE);
  //__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);

  HAL_UARTEx_ReceiveToIdle_IT(&huart2, uart_rxbuf, sizeof(uart_rxbuf));
}


void uart_configure(uint8_t *data) {

  struct uartPacket config = *((struct uartPacket*)data);

  //HAL_UART_DeInit(&huart2);

  uint32_t WordLength;
  uint32_t Parity;
  uint32_t StopBits;
  uint32_t HwFlowCtl = UART_HWCONTROL_NONE;

  switch (config.bits) {
    case 7:
      WordLength = UART_WORDLENGTH_7B;
      break;
    case 8:
      WordLength = UART_WORDLENGTH_8B;
      break;
    case 9:
      WordLength = UART_WORDLENGTH_9B;
      break;
  }

  char parity_str;

  switch (config.parity) {
    case PARITY_EVEN:
      Parity = UART_PARITY_EVEN;
      parity_str = 'E';
      break;
    case PARITY_ODD:
      Parity = UART_PARITY_ODD;
      parity_str = 'O';
      break;
    case PARITY_NONE:
      Parity = UART_PARITY_NONE;
      parity_str = 'N';
      break;
  }

  switch (config.stop_bits) {
    case 0:
      StopBits = UART_STOPBITS_0_5;
      break;
    case 1:
      StopBits = UART_STOPBITS_1;
      break;
    case 2:
      StopBits = UART_STOPBITS_1;
      break;
  }

  if (config.flow_control) {
    HwFlowCtl = UART_HWCONTROL_RTS_CTS;
  }

  if (huart2.Init.BaudRate == config.baud &&
      huart2.Init.WordLength == WordLength &&
      huart2.Init.StopBits == StopBits &&
      huart2.Init.Parity == Parity &&
      huart2.Init.HwFlowCtl == HwFlowCtl) {
      return;
  }

  huart2.Init.BaudRate = config.baud;
  huart2.Init.WordLength = WordLength;
  huart2.Init.StopBits = StopBits;
  huart2.Init.Parity = Parity;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = HwFlowCtl;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  dbg_printf("Reconfiguring UART with %d baud, %d%c%d , %s flow control\n",
    config.baud, config.bits, parity_str, config.stop_bits, config.flow_control ? "" : "no");

  HAL_UART_DeInit(&huart2);

  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }

  UART2_enable_rx_irq();
}