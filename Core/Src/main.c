/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {false, true} bool;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MSB 1U
#define LSB 0U
#define ENCODER_CHANGING_DIRECTLY 0U // Прямой режим смены энкодера (сдвиг нуля в переменной "EncoderPointer" от младшего к старшему биту)
#define ENCODER_CHANGING_REVERSELY 1U // Обратный режим смены энкодера (сдвиг нуля в переменной "EncoderPointer" от старшего к младшему биту)
// НАЧАЛО Главных настроек энкодеров
#define QUANTITY_OF_ENCODERS 16U // Количество энкодеров
#define ENCODER_RESOLUTION 12U // Разрешение энкодера
// КОНЕЦ Главных настроек энкодеров
// НАЧАЛО настроек сдвиговых регистров
#define CLOCK_POLARITY 0U // Полярность сигнала SCK (Если 0, то первоначальное состояние - LOW. Если 1, то первоначальное состояние - HIGH);
#define CLOCK_PHASE 0U // Фазовый сдвиг сигнала SCK (Если 0, то передача бита осуществляется одновременно с изменением полярности SCK. Если 1, то сначала передаётся бит, потом меняется полярность SCK);
#define BIT_DIRECTION MSB // Порядок передачи битов в сдвиговый регистр. Если MSB, то передача данных начинается со старшего бита. Если LSB, то с младшего;
#define ENCODER_CHANGING_MODE ENCODER_CHANGING_DIRECTLY // Режим смены энкодера
#define QUANTITY_OF_ITERS_FOR_CLK_SIGNAL QUANTITY_OF_ENCODERS*2U // Количество итераций для смены сигнала CLK при установке сдвиговых регистров
// КОНЕЦ настроек сдвиговых регистров
#define ENCODER_DUMMY_BITS ENCODER_RESOLUTION < 16U ? 16U - ENCODER_RESOLUTION : 0U // Если разрешение энкодера меньше 16 бит, тогда количество лишних бит с энкодера = 16 - разрешение энкодера;
//																					в противном случае количество лишних бит с энкодера = 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
volatile uint16_t EncoderPointer = 0xFFFE; // Переменная для выбора энкодера. Изначально выбран первый энкодер
bool notdummy; // Изменение полярности сигнала SCK без отправки бита в сдвиговый регистр
volatile uint16_t encData[QUANTITY_OF_ENCODERS] = {0};
extern volatile uint32_t sysTickCount;
short FROM_CONT[160];
short TO_CONT[160];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Метод "SPI_Receive" считывает 16 бит данных в буфер с помощью аппаратного SPI
void SPI_Receive(uint16_t *data/*, uint32_t size*/)
{
	SPI1->CR1 &= ~(1<<6); // Деактивируем аппаратный SPI
	while (((SPI1->SR) & (1<<7))) {};  // Ждём, пока аппаратный SPI не завершит задачу
	SPI1->CR1 |= (1<<6); // Активируем аппаратный SPI
	while (((SPI1->SR) & (1<<7))) {};  // Ждём, пока аппаратный SPI не завершит задачу
	//while (size) {
		SPI1->DR = 0;  // Отправляем пустые данные
		while (((SPI1->SR) & (1<<7))) {};  // Ждём, пока аппаратный SPI не завершит задачу
		*data++ = (SPI1->DR); // Записываем полученные данные в буфер "data"
		while (((SPI1->SR) & (1<<7))) {};  // Ждём, пока аппаратный SPI не завершит задачу
		//size--;
	//}
	SPI1->CR1 &= ~(1<<6); // Деактивируем аппаратный SPI
	while (((SPI1->SR) & (1<<7))) {};  // Ждём, пока аппаратный SPI не завершит задачу
}

// Метод "SetPinToStateOfLastBit" передаёт крайний бит переменной EncoderPtr в входной пин сдвигового регистра
void SetPinToStateOfLastBit(uint16_t EncoderPtr) {
#if BIT_DIRECTION == MSB
		if ((EncoderPtr & 0x8000) == 0x8000) {
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
		}
		EncoderPtr <<= 1;
//		encCntTmp ^= 1;
#elif BIT_DIRECTION == LSB
		if ((EncoderPtr & 1) == 1) {
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
		}
		EncoderPtr >>= 1;
//		encCntTmp ^= 0x8000;
#endif
}

// Метод "ChangeEncoder" меняет значение переменной "EncoderPointer" для выбора следующего энкодера
void ChangeEncoder(/*bool reverse*/) {
#if ENCODER_CHANGING_MODE == ENCODER_CHANGING_DIRECTLY // Если выбран прямой режим смены энкодера (от младшего к старшему биту)
//		if (EncoderCount == 0x7FFF) { // Если 0 в старшем разряд, то переносим его из старшего в младший разряд
//			EncoderCount = 0xFFFE;
//		} else { // иначе сдвигаем 0 влево и устанавливаем 1 в младший разряд
			EncoderPointer <<= 1;
			EncoderPointer |= 1;
//		}
#elif ENCODER_CHANGING_MODE == ENCODER_CHANGING_REVERSELY // Если выбран обратный режим смены энкодера (от старшего к младшему биту)
//		if (EncoderCount == 0xFFFE) { // Если 0 в младшем разряде, то переносим его из младшего в старший разряд
//			EncoderCount = 0x7FFF;
//		} else { // иначе сдвигаем 0 вправо и устанавливаем 1 в старший разряд
			EncoderPointer >>= 1;
			EncoderPointer |= 0x8000;
//		}
#endif
}
// Метод "SetShiftRegisters" устанавливает значение переменной EncoderPointer в сдвиговые регистры для выбора энкодера, с которого будет считано значение
void SetShiftRegisters() {
#if CLOCK_PHASE == 0U
		notdummy = true; // Первый бит передаётся одновременно с изменением полярности сигнала SCK
#else
		notdummy = false; // Передаём сначала бит, потом меняем полярность сигнала SCK
#endif
	uint8_t bits = 0; // Количество бит, отправляемых на сдвиговые регистры
#if CLOCK_PHASE > 0U
	bool firstTick = true; // Переменная, определяющая нулевую итерацию цикла
#endif
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET); // 12-тые пины микросхем(защелка) сдвиговых регистров устанавливаем в LOW
#if CLOCK_POLARITY == 0U
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET); // Устанавливаем SCK в LOW
#else
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET); // Устанавливаем SCK в HIGH
#endif
		do { // Начало цикла
#if CLOCK_PHASE == 0U
				if (notdummy) {
					SetPinToStateOfLastBit(EncoderPointer); // Передаём бит одновременно с изменением полярности сигнала SCK
				}
				notdummy = !notdummy;
				HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3); // Изменяем полярность сигнала SCK
#else
				if (firstTick) { // Если нулевой цикл, то передаём бит без изменения полярности сигнала SCK
					SetPinToStateOfLastBit(EncoderPointer);
					firstTick = false;
				} else { // Если не нулевой цикл, то сначала меняем полярность SCK без передачи бита. Далее снова меняем полярность SCK и одновременно передаём бит
					if (notdummy) {
						SetPinToStateOfLastBit(EncoderPointer);
					}
					notdummy = !notdummy;
					HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3); // Изменяем полярность сигнала SCK
				}
			}
#endif
			bits++;
		} while (bits < QUANTITY_OF_ITERS_FOR_CLK_SIGNAL); // Цикл на каждое изменение состояния SCK сигнала
#if CLOCK_POLARITY == 0U
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET); // Устанавливаем SCK в LOW
#else
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET); // Устанавливаем SCK в HIGH
#endif
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET); // Подаём положительный импульс на 12-тые пины микросхем сдвиговых регистров
			for (uint8_t w = 0; w < 255; w++); // Ждём короткое время
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET); // Сбрасываем положительный импульс на 12-тых пинах микросхем сдвиговых регистров
}

uint16_t GetCurrentEncoderData() {
	uint16_t tmpdata = 0; // Данные с энкодера
	SPI_Receive((uint16_t*)&tmpdata);
	tmpdata >>= ENCODER_DUMMY_BITS; // Убираем лишние биты в младших разрядах
	return tmpdata; // Возвращаем полученные данные
}

void CollectDataFromEncoders() {
	EncoderPointer = 0xFFFE; // Выбор первого энкодера для считывания значения с него
	uint8_t cycles;
#if QUANTITY_OF_ENCODERS <= 32U
		cycles = QUANTITY_OF_ENCODERS;
#else
		cycles = 32U; // Защита от выхода указателя за предел массива
#endif
	for (uint8_t i = 0; i < cycles; i++) { // Цикл для каждого энкодера
		SetShiftRegisters(); // Устанавливаем значения CS на энкодеры побитно из переменной "EncoderPointer" при помощи сдвиговых регистров
 		encData[i] = GetCurrentEncoderData(); // Считываем значение с выбранного энкодера
 		FROM_CONT[i+128] = (short)(encData[i]); // Копируем данные с энкодеров в буфер для отправки на UDP порт
		ChangeEncoder(); // Выбираем следующий энкодер
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SysTick_Config(SystemCoreClock/1000);
  HAL_GPIO_WritePin(LEDG_GPIO_Port, LEDG_Pin, GPIO_PIN_SET);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_LWIP_Init();
  /* USER CODE BEGIN 2 */
  udp_server_init(HAL_GetTick());
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	MX_LWIP_Process();
	sysTickCount = HAL_GetTick();
	periodic_handler(sysTickCount);

	CollectDataFromEncoders(); // записываем данные со всех энкодеров по порядку в массив EncData[]
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(HOLD_GPIO_Port, HOLD_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LEDR_Pin|LEDG_Pin|LEDB_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MOSI_GPIO_Port, MOSI_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SCK_GPIO_Port, SCK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SCS_GPIO_Port, SCS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : HOLD_Pin */
  GPIO_InitStruct.Pin = HOLD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HOLD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_Pin */
  GPIO_InitStruct.Pin = KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LEDR_Pin LEDB_Pin */
  GPIO_InitStruct.Pin = LEDR_Pin|LEDB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : LEDG_Pin */
  GPIO_InitStruct.Pin = LEDG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LEDG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MOSI_Pin */
  GPIO_InitStruct.Pin = MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(MOSI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SCK_Pin */
  GPIO_InitStruct.Pin = SCK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SCK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SCS_Pin */
  GPIO_InitStruct.Pin = SCS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SCS_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
