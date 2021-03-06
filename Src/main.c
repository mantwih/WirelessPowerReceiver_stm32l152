/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32l1xx_hal.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "P9025AC_I2C.h"
#include "JG_BinaryProtocolCommands.h"


#define RXBUFFERSIZE 1


/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim7;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
/* Buffer used for reception */
uint8_t rx_buffer_GV[RXBUFFERSIZE];
uint8_t g_RepeatModeTimerTimedOutFlag;
uint8_t g_RepeatModeTimerTimeout_seconds = 2;
uint8_t g_RepeatModeTimer_seconds = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART4_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM7_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
	uint8_t tmp_head;
	tmp_head = (command_buffer_head_index_GV+1) & COMMAND_BUFFER_MASK;
	if(tmp_head == command_buffer_tail_index_GV )		// If the circular buffer is full (head caught up to tail),
		g_CommandBufferFullFlag = SET;						// set the flag and stop writing new data to buffer
	else
	{
		command_buffer_head_index_GV = tmp_head;
		command_buffer_GV[command_buffer_head_index_GV] = rx_buffer_GV[0];
	}

	g_CommandReceivedCounter = SET;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	 if(htim->Instance == TIM7)
	 {
		 ++g_RepeatModeTimer_seconds;

		 HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
		 g_MeasurementsFlag = SET;

		 if (g_RepeatModeTimer_seconds == g_RepeatModeTimerTimeout_seconds)
		 {
			 g_RepeatModeTimerTimedOutFlag = SET;
			 g_RepeatModeTimer_seconds = 0;
		 }

	 }

}
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM7_Init();

  /* USER CODE BEGIN 2 */

  //Inicjalizacja Timera TIM7
  HAL_TIM_Base_Start_IT(&htim7);
  //Inicjalizacja odbierania komend po UART
  HAL_UART_Receive_IT(&huart2, (uint8_t *)rx_buffer_GV, RXBUFFERSIZE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  uint8_t CurrentCommand = 0;

  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
		if(g_CommandReceivedCounter == SET)
		{
		  JG_CommandBuffer_GetCommand(&CurrentCommand);
		  g_CommandReceivedCounter = RESET;
		  HAL_UART_Receive_IT(&huart2, (uint8_t *)rx_buffer_GV, RXBUFFERSIZE);
		  JG_ProcessCurrentCommand(CurrentCommand);
		}

		if(g_MeasurementsFlag == SET)
		{
		  JG_I2C_ReadMeasurementsFromP9025ACMem_Blocking();
		  JG_I2C_PutMeasurementsValuesToGlobalVariablesFromRaw();
		  JG_I2C_ReadMiscellaneousRegistersFromP9025ACMem_Blocking();
		  JG_I2C_PutMiscellaneousValuesToGlobalVariablesFromRaw();
		  JG_I2C_ReadWPCIDRegistersFromP9025ACMem_Blocking();
		  JG_I2C_PutWPCIDValuesToGlobalVariablesFromRaw();

		  g_MeasurementsFlag = RESET;
		}

		if(g_RepeatModeTimerTimedOutFlag && g_ExtendedRepeatModeFlag)
		{
			HAL_UART_Transmit(&huart2,"======== Measurements ========\n\r", sizeof("======== Measurements ========\n\r")-1,10);
			HAL_UART_Transmit(&huart2,"------------------------------\n\r", sizeof("------------------------------\n\r")-1,10);
			size_t size = sprintf(aTxBuffer_GV, "|     V_rect =  %3.2f V       |\r\n", g_P9025AC_Vrect_Volts);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "|     I_out  =  %4d mA      |\r\n", g_P9025AC_I_out_mA);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "|     f_clk  =   %3d kHz     |\r\n", g_P9025AC_f_clk_Hz);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			HAL_UART_Transmit(&huart2,"------------------------------\n\r", sizeof("------------------------------\n\r")-1,10);
			HAL_UART_Transmit(&huart2,"=========== Miscellaneous Information ===========\n\r", sizeof("=========== Miscellaneous Information ===========\n\r")-1,10);
			HAL_UART_Transmit(&huart2,"-------------------------------------------------\n\r", sizeof("-------------------------------------------------\n\r")-1,10);
			size = sprintf(aTxBuffer_GV, "| V_rect higher than UnderVoltageLockOut   | %d |\n\r", g_PA9025AC_VrectHigherThanUVLOThreshold);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "|     V_rect higher than CLAMP threshold   | %d |\n\r", g_PA9025AC_VrectHigherThanACClampThreshold);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "|             LDO Current Limit Exceeded   | %d |\n\r", g_PA9025AC_LDOCurrentLimitExceeded);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "|                        CHARGE_COMPLETE   | %d |\n\r", g_PA9025AC_ChargeComplete);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "|             Die temperature over 150*C   | %d |\n\r", g_PA9025AC_DieTemperatureOver150Celsius);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			HAL_UART_Transmit(&huart2,"-------------------------------------------------\n\r", sizeof("-------------------------------------------------\n\r")-1,10);
			g_RepeatModeTimerTimedOutFlag = 0;
		}
		if(g_RepeatModeTimerTimedOutFlag && g_RepeatModeFlag)
		{
			size_t size = sprintf(aTxBuffer_GV, "%1.2f %d %d ", g_P9025AC_Vrect_Volts, g_P9025AC_I_out_mA, g_P9025AC_f_clk_Hz);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			size = sprintf(aTxBuffer_GV, "%d %d %d %d %d\r\n", g_PA9025AC_VrectHigherThanUVLOThreshold, g_PA9025AC_VrectHigherThanACClampThreshold, g_PA9025AC_LDOCurrentLimitExceeded, g_PA9025AC_ChargeComplete, g_PA9025AC_DieTemperatureOver150Celsius);
			HAL_UART_Transmit(&huart2,aTxBuffer_GV,size, 10);
			g_RepeatModeTimerTimedOutFlag = 0;
		}

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_SYSCLK, RCC_MCODIV_1);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* I2C1 init function */
static void MX_I2C1_Init(void)
{

  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM7 init function */
static void MX_TIM7_Init(void)
{

  TIM_MasterConfigTypeDef sMasterConfig;

  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 5999;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 999;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* UART4 init function */
static void MX_UART4_Init(void)
{

  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
     PA8   ------> RCC_MCO
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : J2_SENS_Pin J1_SENS_Pin */
  GPIO_InitStruct.Pin = J2_SENS_Pin|J1_SENS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : J1_EN_Pin */
  GPIO_InitStruct.Pin = J1_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J1_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(J1_EN_GPIO_Port, J1_EN_Pin, GPIO_PIN_RESET);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
