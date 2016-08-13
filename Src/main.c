/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
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
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */

//#include "CRC.h"
//#include "serial_terminal.h"
#include <string.h>

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart6_rx;

osThreadId defaultTaskHandle;
osThreadId TXPCHandle;
osThreadId RXPCHandle;
osThreadId RXiNemoHandle;
osThreadId TXMotor1Handle;
osThreadId TXMotor2Handle;
osThreadId RXMotor1Handle;
osThreadId RXMotor2Handle;
osThreadId CombineiNemoHandle;
osThreadId CombineM1Handle;
osThreadId CombineM2Handle;
osMessageQId TXBoxHandle;
osMessageQId TXBoxM1Handle;
osMessageQId TXBoxM2Handle;
osMessageQId RXBoxiNemoHandle;
osMessageQId RXBoxM1Handle;
osMessageQId RXBoxM2Handle;
osMessageQId StatusHandle;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
uint8_t Ts = 5; //Sampling time in ms

uint8_t MW[2];
uint8_t MB[2];

typedef struct {                                 // Message object structure
        float current[2];
        float velocity[2];
        float position[2];
        float dist;
        float acc[3];
        float gyr[3];
        uint8_t MW[2]; //Motor write enabled
        uint8_t MB[2]; //Motor bridge enabled
} T_MEAS;

T_MEAS    *mptr;
T_MEAS    *rptr;

//PC Buffer
uint8_t TXBuf[10];
uint8_t RXBuf[10];

//Motor Buffer
uint8_t TXBufM1[14];
uint8_t TXBufM2[14];
uint8_t TXM1Complete=0;
uint8_t TXM2Complete=0;

uint8_t RXBufM1[14];
uint8_t RXBufM2[14];

unsigned char Motor_Success[8] = {0xA5, 0xFF, 0x00, 0x01, 0x00, 0x00, 0xCF, 0xB6};
//These arrays hold all the necessary hex commands to send to motor controllers
uint8_t Write_Access[12];
uint8_t Enable_Bridge[12];
uint8_t Disable_Bridge[12];

uint8_t Current_Command[14];
uint8_t Position_Command[14];

uint8_t Read_Current[8];
uint8_t Read_Velocity[8];
uint8_t Read_Position[8];

/* WHAT ARE ALL THE USARTS AND TIMERS USED FOR AND WHICH PINS DO THEY USE?
 *
 * USART1 = COMMS between the PC and STM	---		PB6 (TX)	 PB7 (RX)
 * USART2 = COMMS to Motor Controller 1		---		PA2 (TX)	 PA3 (RX)
 * USART3 = COMMS to Motor Controller 2		---		PC10 (TX)    PC11 (RX)
 * USART6 = COMMS to iNEMO					---		PC6 (TX)	 PC7 (RX)
 *
 * TIM1 = Timer for Center Boom Encoder		---		PE9 (A)	     PE11 (B)
 * TIM2 = Timer for Pitch Encoder			---		PA15 (A)	 PB3 (B)
 * TIM3 = Used for 5ms interrupts
 * TIM4 = Timer for Linear Encoder			---		PD12 (A)     PD13 (B)
 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void const * argument);
void StartTXPC(void const * argument);
void StartRXPC(void const * argument);
void StartRXiNemo(void const * argument);
void StartTXMotor1(void const * argument);
void StartTXMotor2(void const * argument);
void StartRXMotor1(void const * argument);
void StartRXMotor2(void const * argument);
void StartCombineiNemo(void const * argument);
void StartCombineM1(void const * argument);
void StartCombineM2(void const * argument);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
osPoolDef(mpool, 16, T_MEAS);                    // Define memory pool
osPoolId mpool;

//Select Call-backs functions called after Transfer complete and Transfer error
//hdma_usart1_rx;
void hdma_usart1_rx_cmplt(DMA_HandleTypeDef *hdma);
//hdma_usart1_tx;
void hdma_usart1_tx_cmplt(DMA_HandleTypeDef *hdma);
//hdma_usart2_rx;
void hdma_usart2_rx_cmplt(DMA_HandleTypeDef *hdma);
//hdma_usart2_tx;
void hdma_usart2_tx_cmplt(DMA_HandleTypeDef *hdma);
//hdma_usart3_rx;
void hdma_usart3_rx_cmplt(DMA_HandleTypeDef *hdma);
//hdma_usart3_tx;
void hdma_usart3_tx_cmplt(DMA_HandleTypeDef *hdma);
//hdma_usart6_rx;
void hdma_usart6_rx_cmplt(DMA_HandleTypeDef *hdma);

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

static void DMA_Start(void);

void Motor_Init(void); //Initialises motors
void Motor_Kill(void);
void Motor_Commands(void); //Holds all commands

void ClearBuf(uint8_t buf[], uint8_t size); //Set buffer contents to 0
void SetBuf(uint8_t buf[], uint8_t set[], uint8_t size); //Set buf[] to set[]
void SetBytes(uint8_t buf[], uint8_t pos1, uint8_t val1, uint8_t pos2, uint8_t val2, uint8_t pos3, uint8_t val3, uint8_t pos4, uint8_t val4); //For setting current commands, set pos to NULL to omit

static void dmaError(DMA_HandleTypeDef *hdma);
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
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
        Motor_Commands();
        DMA_Start();
        Motor_Init();
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
        /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
        /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
        /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityBelowNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of TXPC */
  osThreadDef(TXPC, StartTXPC, osPriorityRealtime, 0, 128);
  TXPCHandle = osThreadCreate(osThread(TXPC), NULL);

  /* definition and creation of RXPC */
  osThreadDef(RXPC, StartRXPC, osPriorityRealtime, 0, 128);
  RXPCHandle = osThreadCreate(osThread(RXPC), NULL);

  /* definition and creation of RXiNemo */
  osThreadDef(RXiNemo, StartRXiNemo, osPriorityNormal, 0, 128);
  RXiNemoHandle = osThreadCreate(osThread(RXiNemo), NULL);

  /* definition and creation of TXMotor1 */
  osThreadDef(TXMotor1, StartTXMotor1, osPriorityHigh, 0, 128);
  TXMotor1Handle = osThreadCreate(osThread(TXMotor1), NULL);

  /* definition and creation of TXMotor2 */
  osThreadDef(TXMotor2, StartTXMotor2, osPriorityHigh, 0, 128);
  TXMotor2Handle = osThreadCreate(osThread(TXMotor2), NULL);

  /* definition and creation of RXMotor1 */
  osThreadDef(RXMotor1, StartRXMotor1, osPriorityAboveNormal, 0, 128);
  RXMotor1Handle = osThreadCreate(osThread(RXMotor1), NULL);

  /* definition and creation of RXMotor2 */
  osThreadDef(RXMotor2, StartRXMotor2, osPriorityAboveNormal, 0, 128);
  RXMotor2Handle = osThreadCreate(osThread(RXMotor2), NULL);

  /* definition and creation of CombineiNemo */
  osThreadDef(CombineiNemo, StartCombineiNemo, osPriorityRealtime, 0, 128);
  CombineiNemoHandle = osThreadCreate(osThread(CombineiNemo), NULL);

  /* definition and creation of CombineM1 */
  osThreadDef(CombineM1, StartCombineM1, osPriorityRealtime, 0, 128);
  CombineM1Handle = osThreadCreate(osThread(CombineM1), NULL);

  /* definition and creation of CombineM2 */
  osThreadDef(CombineM2, StartCombineM2, osPriorityRealtime, 0, 128);
  CombineM2Handle = osThreadCreate(osThread(CombineM2), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
        /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Create the queue(s) */
  /* definition and creation of TXBox */
  osMessageQDef(TXBox, 6, float);
  TXBoxHandle = osMessageCreate(osMessageQ(TXBox), NULL);

  /* definition and creation of TXBoxM1 */
  osMessageQDef(TXBoxM1, 16, uint16_t);
  TXBoxM1Handle = osMessageCreate(osMessageQ(TXBoxM1), NULL);

  /* definition and creation of TXBoxM2 */
  osMessageQDef(TXBoxM2, 16, uint16_t);
  TXBoxM2Handle = osMessageCreate(osMessageQ(TXBoxM2), NULL);

  /* definition and creation of RXBoxiNemo */
  osMessageQDef(RXBoxiNemo, 16, uint16_t);
  RXBoxiNemoHandle = osMessageCreate(osMessageQ(RXBoxiNemo), NULL);

  /* definition and creation of RXBoxM1 */
  osMessageQDef(RXBoxM1, 16, uint16_t);
  RXBoxM1Handle = osMessageCreate(osMessageQ(RXBoxM1), NULL);

  /* definition and creation of RXBoxM2 */
  osMessageQDef(RXBoxM2, 16, uint16_t);
  RXBoxM2Handle = osMessageCreate(osMessageQ(RXBoxM2), NULL);

  /* definition and creation of Status */
  osMessageQDef(Status, 16, uint8_t);
  StatusHandle = osMessageCreate(osMessageQ(Status), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
        /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
 

  /* Start scheduler */
  osKernelStart();
  
  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
        while (1)
        {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

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

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
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

/* USART3 init function */
static void MX_USART3_UART_Init(void)
{

  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART6 init function */
static void MX_USART6_UART_Init(void)
{

  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
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
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  /* DMA2_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

/** Pinout Configuration
*/
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

}

/* USER CODE BEGIN 4 */

static void DMA_Start(void){

        //HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);
        //HAL_UART_Transmit_DMA(&huart1, TXBuf, (uint16_t)(sizeof(TXBuf)/sizeof(TXBuf[0])));
        //HAL_UART_Receive_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);
        //static void DMA_SetConfig(DMA_HandleTypeDef *hdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t DataLength)
        //HAL_StatusTypeDef HAL_DMA_Start(DMA_HandleTypeDef *hdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t DataLength)

        // //DMA_HandleTypeDef hdma_usart1_rx;
        // if(HAL_UART_Receive_DMA(&huart1, RXBuf, 10) != HAL_OK) {Error_Handler(); }
        // //DMA_HandleTypeDef hdma_usart1_tx;
        // if(HAL_UART_Transmit_DMA(&huart1, TXBuf, 10) != HAL_OK) {Error_Handler(); }
        // //DMA_HandleTypeDef hdma_usart2_rx;
        // //DMA_HandleTypeDef hdma_usart2_tx;
        // if(HAL_UART_Transmit_DMA(&huart2, TXBufM1, 14) != HAL_OK) {Error_Handler(); }
        // //DMA_HandleTypeDef hdma_usart3_rx;
        // //DMA_HandleTypeDef hdma_usart3_tx;
        // if(HAL_UART_Transmit_DMA(&huart3, TXBufM2, 14) != HAL_OK) {Error_Handler(); }
        // //DMA_HandleTypeDef hdma_usart6_rx;
        // //DMA_HandleTypeDef hdma_usart6_tx;




        //End of DMA transfer callbacks
        //HAL_DMA_RegisterCallback(DMA_HandleTypeDef *hdma, HAL_DMA_CallbackIDTypeDef CallbackID, void (* pCallback)(DMA_HandleTypeDef *_hdma));

//       HAL_DMA_RegisterCallback(&hdma_usart1_rx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart1_rx_cmplt);
//       HAL_DMA_RegisterCallback(&hdma_usart1_tx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart1_tx_cmplt);
//       HAL_DMA_RegisterCallback(&hdma_usart2_rx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart2_rx_cmplt);
//       HAL_DMA_RegisterCallback(&hdma_usart2_tx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart2_tx_cmplt);
//       HAL_DMA_RegisterCallback(&hdma_usart3_rx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart3_rx_cmplt);
//       HAL_DMA_RegisterCallback(&hdma_usart3_tx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart3_tx_cmplt);
//       HAL_DMA_RegisterCallback(&hdma_usart6_rx, HAL_DMA_XFER_CPLT_CB_ID , hdma_usart6_rx_cmplt);

        // hdma_usart1_rx.XferCpltCallback = hdma_usart1_rx_cmplt;
        // hdma_usart1_tx.XferCpltCallback = hdma_usart1_tx_cmplt;
        // hdma_usart2_rx.XferCpltCallback = hdma_usart2_rx_cmplt;
        // hdma_usart2_tx.XferCpltCallback = hdma_usart2_tx_cmplt;
        // hdma_usart3_rx.XferCpltCallback = hdma_usart3_rx_cmplt;
        // hdma_usart3_tx.XferCpltCallback = hdma_usart3_tx_cmplt;
        // hdma_usart6_rx.XferCpltCallback = hdma_usart6_rx_cmplt;

}

// uint8_t Write_Access[11];
// uint8_t Enable_Bridge[11];
// uint8_t Disable_Bridge[20];
//
// uint8_t Current_Command[13];
// uint8_t Position_Command[13];
//
// uint8_t Read_Current[7];
// uint8_t Read_Velocity[7];
// uint8_t Read_Position[7];

void Motor_Init(void){

	SetBuf(TXBufM1, Write_Access, 12);
		SetBuf(TXBufM2, Write_Access, 12);

		//HAL_DMA_Start_IT(&hdma_usart2_tx, (uint32_t)&TXBufM1, (uint32_t) &(huart2.Instance->DR), 14);
		//HAL_DMA_Start_IT(&hdma_usart3_tx, (uint32_t)&TXBufM2, (uint32_t) &(huart3.Instance->DR), 14);

            //if(HAL_UART_Transmit_DMA(&huart2, TXBufM1, 14) != HAL_OK) { Error_Handler();}
            //while(TXM1Complete!=1);
            //if(HAL_UART_Transmit_DMA(&huart3, TXBufM2, 14) != HAL_OK) { Error_Handler();}
            //while(TXM2Complete!=1);

        //__HAL_DMA_DISABLE(&hdma_usart3_tx);
        //__HAL_DMA_ENABLE(&hdma_usart3_tx);
        //while(HAL_DMA_GetState(&hdma_usart2_tx) == HAL_DMA_STATE_BUSY);

        //Gain write access and enable bridge
        //if(HAL_UART_DMAPause(&huart2) != HAL_OK){Error_Handler();}
        //if(HAL_UART_DMAPause(&huart3) != HAL_OK){Error_Handler();}

        //MX_USART2_UART_Init();
        //MX_USART3_UART_Init();

        //__HAL_UART_ENABLE(&huart2);
        //__HAL_UART_ENABLE(&huart3);

        //HAL_UART_Init(&huart2);
        //HAL_UART_Init(&huart3);

        //TODO Try osDelay()...?

//		    if(HAL_UART_DMAPause(&huart2) != HAL_OK){Error_Handler();}
//		    if(HAL_UART_DMAPause(&huart3) != HAL_OK){Error_Handler();}
//        SetBuf(TXBufM1, Write_Access, 12);
//        SetBuf(TXBufM2, Write_Access, 12);
//        HAL_UART_Transmit(&huart2, TXBufM1, 14, 1000);
//        HAL_UART_Transmit(&huart3, TXBufM2, 14, 1000);
//        osDelay(500);
//        SetBuf(TXBufM1, Enable_Bridge, 12);
//        SetBuf(TXBufM2, Enable_Bridge, 12);
//        HAL_UART_Transmit(&huart2, TXBufM1, 14, 1000);
//        HAL_UART_Transmit(&huart3, TXBufM2, 14, 1000);
//        osDelay(500);
//        SetBuf(TXBufM1, Current_Command, 12);
//        SetBuf(TXBufM2, Current_Command, 12);
//        HAL_UART_DMAResume(&huart2);
//        HAL_UART_DMAResume(&huart3);

        //HAL_DMA_Start(&hdma_usart2_tx, (uint32_t)&TXBufM1, (uint32_t)huart2.Instance->DR, (uint32_t)14);
        //TXM1Complete = 0;
        //if(HAL_UART_Transmit_DMA(&huart2, TXBufM1, 14) != HAL_OK) { Error_Handler();}
        //while(huart2.gState != HAL_UART_STATE_READY);
        //osDelay(1000);
        //while(HAL_DMA_GetState(&hdma_usart2_tx) != HAL_DMA_STATE_READY);
        //while(TXM1Complete!=1);
        //while(huart2->gState != HAL_UART_STATE_READY){};
        //while(HAL_UART_GetState(&huart2) != HAL_UART_STATE_READY);
        //while(__HAL_DMA_GET_IT_SOURCE(&hdma_usart2_tx, DMA_IT_TC) != RESET);
        //HAL_UART_Transmit(&huart2, TXBufM1, 14, 1000);

        //SetBuf(TXBufM2, Write_Access, 12);
        //TXM2Complete = 0;
        //HAL_DMA_Start(&hdma_usart3_tx, (uint32_t)&TXBufM2, (uint32_t)huart3.Instance->DR, (uint32_t)14);
        //if(HAL_UART_Transmit_DMA(&huart3, TXBufM2, 14) != HAL_OK) { Error_Handler();}
        //while(huart3.gState != HAL_UART_STATE_READY);
        //osDelay(1000);
        //while(HAL_DMA_GetState(&hdma_usart3_tx) != HAL_DMA_STATE_READY);
        //while(TXM2Complete!=1);
        //while(HAL_UART_GetState(&huart3) != HAL_UART_STATE_READY);
        //while(__HAL_DMA_GET_IT_SOURCE(&hdma_usart3_tx, DMA_IT_TC) != RESET);
        //HAL_UART_Transmit(&huart3, TXBufM2, 14, 1000);

        //SetBuf(TXBufM1, Enable_Bridge, 12);
        //TXM1Complete = 0;
        //if(HAL_UART_Transmit_DMA(&huart2, TXBufM1, 14) != HAL_OK) { Error_Handler();}
        //while(huart2.gState != HAL_UART_STATE_READY);
        //osDelay(1000);
        //while(HAL_DMA_GetState(&hdma_usart2_tx) != HAL_DMA_STATE_READY);
        //while(TXM1Complete!=1);
        //HAL_UART_Transmit(&huart2, TXBufM1, 14, 1000);

        //SetBuf(TXBufM2, Enable_Bridge, 12);
        //TXM2Complete = 0;
        //if(HAL_UART_Transmit_DMA(&huart3, TXBufM2, 14) != HAL_OK) { Error_Handler();}
        //while(huart3.gState != HAL_UART_STATE_READY);
        //osDelay(1000);
        //while(HAL_DMA_GetState(&hdma_usart3_tx) != HAL_DMA_STATE_READY);
        //while(TXM2Complete!=1);
        //HAL_UART_Transmit(&huart3, TXBufM2, 14, 1000);

        //SetBuf(TXBufM1, Current_Command, 12);
        //SetBuf(TXBufM2, Current_Command, 12);
        //HAL_UART_DMAResume(&huart2);
        //HAL_UART_DMAResume(&huart3);

        //if(HAL_UART_Transmit_DMA(&huart2, TXBufM1, 14) != HAL_OK) {Error_Handler(); }
        //if(HAL_UART_Transmit_DMA(&huart3, TXBufM2, 14) != HAL_OK) {Error_Handler(); }
}

void Motor_Kill(void){
        //Disable bridge
        HAL_UART_DMAStop(&huart2);
        HAL_UART_DMAStop(&huart3);

        SetBuf(TXBufM1, Disable_Bridge, 12);
        HAL_UART_Transmit(&huart2, TXBufM1, 14, 100);

        SetBuf(TXBufM2, Disable_Bridge, 12);
        HAL_UART_Transmit(&huart3, TXBufM2, 14, 100);

        __HAL_DMA_DISABLE(&hdma_usart2_tx);
        __HAL_DMA_DISABLE(&hdma_usart3_tx);
}

void SetCurrent(uint8_t buf[]){
        SetBuf(buf, Current_Command, 14);
        SetBytes(buf, 8, RXBuf[0], 9, RXBuf[1], 10, RXBuf[2], 11, RXBuf[3]);
}

void Motor_Commands(void){
        Write_Access[0] = 0xA5;
        Write_Access[1] = 0x3F;
        Write_Access[2] = 0x02;
        Write_Access[3] = 0x07;
        Write_Access[4] = 0x00;
        Write_Access[5] = 0x01;
        Write_Access[6] = 0xB3;
        Write_Access[7] = 0xE7;
        Write_Access[8] = 0x0F;
        Write_Access[9] = 0x00;
        Write_Access[10] = 0x10;
        Write_Access[11] = 0x3E;

        Enable_Bridge[0] = 0xA5;
        Enable_Bridge[1] = 0x3F;
        Enable_Bridge[2] = 0x02;
        Enable_Bridge[3] = 0x01;
        Enable_Bridge[4] = 0x00;
        Enable_Bridge[5] = 0x01;
        Enable_Bridge[6] = 0x01;
        Enable_Bridge[7] = 0x47;
        Enable_Bridge[8] = 0x00;
        Enable_Bridge[9] = 0x00;
        Enable_Bridge[10] = 0x00;
        Enable_Bridge[11] = 0x00;

        Disable_Bridge[0] = 0xA5;
        Disable_Bridge[1] = 0x3F;
        Disable_Bridge[2] = 0x02;
        Disable_Bridge[3] = 0x01;
        Disable_Bridge[4] = 0x00;
        Disable_Bridge[5] = 0x01;
        Disable_Bridge[6] = 0x01;
        Disable_Bridge[7] = 0x47;
        Disable_Bridge[8] = 0x01;
        Disable_Bridge[9] = 0x00;
        Disable_Bridge[10] = 0x33;
        Disable_Bridge[11] = 0x31;

        Current_Command[0] = 0xA5;
        Current_Command[1] = 0x3F;
        Current_Command[2] = 0x02;
        Current_Command[3] = 0x45;
        Current_Command[4] = 0x00;
        Current_Command[5] = 0x02;
        Current_Command[6] = 0xF0;
        Current_Command[7] = 0x49;
        Current_Command[8] = 0x48; //Data to be set
        Current_Command[9] = 0x01; //Data to be set
        Current_Command[10] = 0x00; //Data to be set
        Current_Command[11] = 0x00; //Data to be set
        Current_Command[12] = 0xDC;
        Current_Command[13] = 0x6F;

        Read_Current[0] = 0xA5;
        Read_Current[1] = 0x3F;
        Read_Current[2] = 0x01;
        Read_Current[3] = 0x10;
        Read_Current[4] = 0x03;
        Read_Current[5] = 0x01;
        Read_Current[6] = 0xBB;
        Read_Current[7] = 0x9B;

        Read_Velocity[0] = 0xA5;
        Read_Velocity[1] = 0x3F;
        Read_Velocity[2] = 0x01;
        Read_Velocity[3] = 0x11;
        Read_Velocity[4] = 0x02;
        Read_Velocity[5] = 0x02;
        Read_Velocity[6] = 0x8F;
        Read_Velocity[7] = 0xF9;

        Read_Position[0] = 0xA5;
        Read_Position[1] = 0x3F;
        Read_Position[2] = 0x01;
        Read_Position[3] = 0x12;
        Read_Position[4] = 0x00;
        Read_Position[5] = 0x02;
        Read_Position[6] = 0xB0;
        Read_Position[7] = 0xCB;
}

void ClearBuf(uint8_t buf[], uint8_t size){
        uint8_t i;
        for (i = 0; i < size; i++) {
                buf[i] = 0;
        }
}

void SetBuf(uint8_t buf[], uint8_t set[], uint8_t size){
        ClearBuf(buf, 14);
        uint8_t i;
        for (i = 0; i < size; i++) {
                buf[i] = set[i];
        }
}

//void ClearBit; //TODO
//void SetBit; //TODO

void SetBytes(uint8_t buf[], uint8_t pos1, uint8_t val1, uint8_t pos2, uint8_t val2, uint8_t pos3, uint8_t val3, uint8_t pos4, uint8_t val4){
        if (pos1 != NULL) {buf[pos1] = buf[val1]; }
        if (pos2 != NULL) {buf[pos2] = buf[val2]; }
        if (pos3 != NULL) {buf[pos3] = buf[val3]; }
        if (pos4 != NULL) {buf[pos4] = buf[val4]; }
}

//Select Call-backs functions called after Transfer complete and Transfer error
//hdma_usart1_rx;
void hdma_usart1_rx_cmplt(DMA_HandleTypeDef *hdma){
        __NOP();
}
//hdma_usart1_tx;
void hdma_usart1_tx_cmplt(DMA_HandleTypeDef *hdma){
        __NOP();
}
//hdma_usart2_rx;
void hdma_usart2_rx_cmplt(DMA_HandleTypeDef *hdma){
        __NOP();
}
//hdma_usart2_tx;
void hdma_usart2_tx_cmplt(DMA_HandleTypeDef *hdma){
//         HAL_UART_DMAPause(&huart2);
//         if(TXCountM1 == 0) {SetBuf(TXBufM1, Write_Access, 12); osDelay(1000);}
//         if(TXCountM1 == 1) {SetBuf(TXBufM1, Enable_Bridge, 12); osDelay(1000);}
//         if(TXCountM1 > 1) {SetBuf(TXBufM1, Current_Command, 14); }
//         TXCountM1++;
//         HAL_UART_DMAResume(&huart2);
	TXM1Complete=1;
        //__NOP();
}
//hdma_usart3_rx;
void hdma_usart3_rx_cmplt(DMA_HandleTypeDef *hdma){
        __NOP();
}
//hdma_usart3_tx;
void hdma_usart3_tx_cmplt(DMA_HandleTypeDef *hdma){
//         HAL_UART_DMAPause(&huart3);
//         if(TXCountM2 == 0) {SetBuf(TXBufM2, Write_Access, 12); osDelay(1000);}
//         if(TXCountM2 == 1) {SetBuf(TXBufM2, Enable_Bridge, 12); osDelay(1000);}
//         if(TXCountM2 > 1) {SetBuf(TXBufM2, Current_Command, 14); }
//         TXCountM2++;
//         HAL_UART_DMAResume(&huart3);
	TXM2Complete=1;
}
//hdma_usart6_rx;
void hdma_usart6_rx_cmplt(DMA_HandleTypeDef *hdma){
        __NOP();
}

static void dmaError(DMA_HandleTypeDef *hdma){
        printf("DMA Error!");
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	BaseType_t xHigherPriorityTaskWoken;
	/* We have not woken a task at the start of the ISR. */
	    xHigherPriorityTaskWoken = pdFALSE;
    TXM1Complete=1;
    xQueueSendFromISR( StatusHandle, &TXM1Complete, &xHigherPriorityTaskWoken );
    TXM2Complete=1;

    //Now the buffer is empty we can switch context if necessary.
        if( xHigherPriorityTaskWoken )
        {
        	/* signal end-of-irq and possible reschedule point */
        	  portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
        }
}

// static void UART_DMATransmitCplt(DMA_HandleTypeDef *hdma)
// {
//         /* Set transmission flag: transfer complete + clear tx buf*/
//         if(hdma == hdma_usart2_tx) {
//                 ClearBuf(TXBufM1, 14);
//                 //huart->gState = HAL_UART_STATE_READY;
//         }
//
//         /* Set transmission flag: transfer complete + clear tx buf*/
//         if(hdma == hdma_usart3_tx) {
//                 ClearBuf(TXBufM2, 14);
//                 //huart->gState = HAL_UART_STATE_READY;
//         }
// }

/* USER CODE END 4 */

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{

  /* USER CODE BEGIN 5 */
        /* Infinite loop */
//        osDelay(5000);
//        Motor_Init();
//        osDelay(10000);
//        Motor_Kill();
        // char str[32];
        // uint8_t tt = 0xAA;
        // sprintf(str, "%d", tt);
        // HAL_UART_Transmit(&huart1,(uint8_t *)str, 100,500);
        // osDelay(100);
        // HAL_UART_Transmit_DMA(&huart1,(uint8_t *)test, strlen(test));
        for(;; )
        {

                osDelay(500); //TODO Remove
        }
  /* USER CODE END 5 */ 
}

/* StartTXPC function */
void StartTXPC(void const * argument)
{
  /* USER CODE BEGIN StartTXPC */
        //osDelay(1000);
        //Motor_Init();
		//uint8_t i;

        //Motor_Kill();
        /* Infinite loop */
        for(;; )
        {
                //HAL_UART_Transmit_DMA(&huart1,(uint8_t *)TXBuf, strlen(TXBuf)); //TODO ...
//        	if(i==1){
//        	RXBuf[0] = 0x48;
//        	RXBuf[1] = 0x01;
//        	RXBuf[2] = 0;
//        	RXBuf[3] = 0;
//        	i=0;
//        	}
//
//        	if(i==0){
//        	RXBuf[0] = 0;
//        	RXBuf[1] = 0;
//        	RXBuf[2] = 0;
//        	RXBuf[3] = 0;
//        	i=1;;
//        	}

//        	SetCurrent(TXBufM1);
//        	SetCurrent(TXBufM2);

        	uint8_t TXM1Complete = 0;
        	                if(HAL_UART_Transmit_DMA(&huart2, TXBufM1, 14) != HAL_OK) { Error_Handler();}
        	                while(TXM1Complete!=1){xQueueReceive(StatusHandle, &TXM1Complete, 5);};
        	                osDelay(5); //TODO Remove
        }
  /* USER CODE END StartTXPC */
}

/* StartRXPC function */
void StartRXPC(void const * argument)
{
  /* USER CODE BEGIN StartRXPC */
        /* Infinite loop */
        for(;; )
        {
                // mptr = osPoolAlloc(mpool);               // Allocate memory for the message
                //                                          //TODO Put code here: mptr->voltage = 223.72;
                // osMessagePut(CombineBoxHandle, (uint32_t)mptr, osWaitForever); // Send Message
                // osThreadYield(); // Cooperative multitasking
                // // We are done here, exit this thread
                osDelay(Ts);
        }
  /* USER CODE END StartRXPC */
}

/* StartRXiNemo function */
void StartRXiNemo(void const * argument)
{
  /* USER CODE BEGIN StartRXiNemo */
        /* Infinite loop */
        for(;; )
        {
                /*mptr = osPoolAlloc(mpool);         // Allocate memory for the message
                   mptr->acc[0] = 1;                                   //TODO Put code here: mptr->voltage = 223.72;
                   mptr->acc[1] = 2;
                   mptr->acc[2] = 1;
                   mptr->gyr[0] = 2;
                   mptr->gyr[1] = 1;
                   mptr->gyr[2] = 2;
                   mptr->dist = 1;
                   osMessagePut(RXBoxiNemoHandle, (uint32_t)mptr, osWaitForever); // Send Message
                   osThreadYield(); // Cooperative multitasking
                   // We are done here, exit this thread*/
                osDelay(Ts);
        }
  /* USER CODE END StartRXiNemo */
}

/* StartTXMotor1 function */
void StartTXMotor1(void const * argument)
{
  /* USER CODE BEGIN StartTXMotor1 */
        /* Infinite loop */
        for(;; )
        {
                osDelay(Ts);
        }
  /* USER CODE END StartTXMotor1 */
}

/* StartTXMotor2 function */
void StartTXMotor2(void const * argument)
{
  /* USER CODE BEGIN StartTXMotor2 */
        /* Infinite loop */
        for(;; )
        {
                osDelay(Ts);
        }
  /* USER CODE END StartTXMotor2 */
}

/* StartRXMotor1 function */
void StartRXMotor1(void const * argument)
{
  /* USER CODE BEGIN StartRXMotor1 */
        /* Infinite loop */
        for(;; )
        {
                /*mptr = osPoolAlloc(mpool);         // Allocate memory for the message
                   mptr->current[0] = 5.5;
                   mptr->velocity[0] = 5.5;
                   mptr->position[0] = 5.5;                                   //TODO Put code here: mptr->voltage = 223.72;
                   osMessagePut(RXBoxM1Handle, (uint32_t)mptr, osWaitForever); // Send Message
                   osThreadYield(); // Cooperative multitasking
                   // We are done here, exit this thread*/
                osDelay(Ts);
        }
  /* USER CODE END StartRXMotor1 */
}

/* StartRXMotor2 function */
void StartRXMotor2(void const * argument)
{
  /* USER CODE BEGIN StartRXMotor2 */
        /* Infinite loop */
        for(;; )
        {
                /* mptr = osPoolAlloc(mpool);   // Allocate memory for the message
                   mptr->current[1] = 7.7;
                   mptr->velocity[1] = 7.7;
                   mptr->position[1] = 7.7;                             //TODO Put code here: mptr->voltage = 223.72;
                   osMessagePut(RXBoxM2Handle, (uint32_t)mptr, osWaitForever); // Send Message
                   osThreadYield(); // Cooperative multitasking
                   // We are done here, exit this thread*/
                osDelay(Ts);
        }
  /* USER CODE END StartRXMotor2 */
}

/* StartCombineiNemo function */
void StartCombineiNemo(void const * argument)
{
  /* USER CODE BEGIN StartCombineiNemo */
        /* Infinite loop */
        //osEvent evt;
        for(;; )
        {
                /*evt = osMessageGet(RXBoxiNemoHandle, osWaitForever); // wait for message
                   if (evt.status == osEventMessage) {
                        rptr = evt.value.p;

                        // TXBuf[7] = rptr->dist; //TODO
                        // TXBuf[8] = rptr->acc[0];
                        // TXBuf[9] = rptr->acc[1];
                        // TXBuf[10] = rptr->acc[2];
                        // TXBuf[11] = rptr->gyr[0];
                        // TXBuf[12] = rptr->gyr[1];
                        // TXBuf[13] = rptr->gyr[2];

                        osPoolFree(mpool, rptr); // free memory allocated for message
                   }*/
        	osDelay(Ts);
        }
  /* USER CODE END StartCombineiNemo */
}

/* StartCombineM1 function */
void StartCombineM1(void const * argument)
{
  /* USER CODE BEGIN StartCombineM1 */
        /* Infinite loop */
        //osEvent evt;
        for(;; )
        {
                /*evt = osMessageGet(RXBoxM1Handle, osWaitForever); // wait for message
                   if (evt.status == osEventMessage) {
                        rptr = evt.value.p;

                        // TXBuf[1] = rptr->current[0]; //TODO
                        // TXBuf[3] = rptr->velocity[0];
                        // TXBuf[5] = rptr->position[0];

                        osPoolFree(mpool, rptr); // free memory allocated for message
                   }*/
        	osDelay(Ts);
        }
  /* USER CODE END StartCombineM1 */
}

/* StartCombineM2 function */
void StartCombineM2(void const * argument)
{
  /* USER CODE BEGIN StartCombineM2 */
        /* Infinite loop */
        //osEvent evt;
        for(;; )
        {
                /*evt = osMessageGet(RXBoxM2Handle, osWaitForever); // wait for message
                   if (evt.status == osEventMessage) {
                        rptr = evt.value.p;

                        // TXBuf[2] = rptr->current[1]; //TODO
                        // TXBuf[4] = rptr->velocity[1];
                        // TXBuf[6] = rptr->position[1];

                        osPoolFree(mpool, rptr); // free memory allocated for message
                   }*/
        	osDelay(Ts);
        }
  /* USER CODE END StartCombineM2 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
/* USER CODE BEGIN Callback 0 */

/* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
/* USER CODE BEGIN Callback 1 */

/* USER CODE END Callback 1 */
}

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
