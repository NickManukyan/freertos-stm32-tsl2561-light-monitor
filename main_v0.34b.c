/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "tsl2561.h"            // TSL2561 sensor driver header (implement or use library)
#include "lcd_i2c.h"            // I2C LCD driver header (implement or use library)

/* Private variables ---------------------------------------------------------*/
osThreadId_t sensorTaskHandle;
osThreadId_t displayTaskHandle;
osThreadId_t ledTaskHandle;
osMessageQueueId_t sensorDataQueueHandle;

typedef struct {
  uint32_t lux;
} SensorData_t;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);

void StartSensorTask(void *argument);
void StartDisplayTask(void *argument);
void StartLedTask(void *argument);

/* Peripheral handles */
I2C_HandleTypeDef hi2c1;

/* Main function --------------------------------------------------------------*/
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();

  lcd_init(&hi2c1);         // Initialize LCD (implement lcd_i2c library)
  tsl2561_init(&hi2c1);     // Initialize TSL2561 sensor (implement tsl2561 library)

  /* Create message queue for sensor data */
  sensorDataQueueHandle = osMessageQueueNew(5, sizeof(SensorData_t), NULL);

  /* Create tasks */
  const osThreadAttr_t sensorTask_attributes = {
    .name = "SensorTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 256
  };
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  const osThreadAttr_t displayTask_attributes = {
    .name = "DisplayTask",
    .priority = (osPriority_t) osPriorityBelowNormal,
    .stack_size = 256
  };
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);

  const osThreadAttr_t ledTask_attributes = {
    .name = "LedTask",
    .priority = (osPriority_t) osPriorityLow,
    .stack_size = 128
  };
  ledTaskHandle = osThreadNew(StartLedTask, NULL, &ledTask_attributes);

  /* Start scheduler */
  osKernelStart();

  Error_Handler();
}

/* Sensor task: reads TSL2561 lux data every 1 second and sends to queue */
void StartSensorTask(void *argument)
{
  SensorData_t sensorData;
  for(;;)
  {
    if(tsl2561_read_lux(&sensorData.lux) == HAL_OK)
    {
      osMessageQueuePut(sensorDataQueueHandle, &sensorData, 0, 0);
    }
    osDelay(1000);
  }
}

/* Display task: receives sensor data and updates LCD */
void StartDisplayTask(void *argument)
{
  SensorData_t sensorData;
  char line1[17];

  for(;;)
  {
    if(osMessageQueueGet(sensorDataQueueHandle, &sensorData, NULL, osWaitForever) == osOK)
    {
      snprintf(line1, sizeof(line1), "Light: %lu lux", sensorData.lux);

      lcd_clear();
      lcd_set_cursor(0, 0);
      lcd_print(line1);
    }
  }
}

/* LED task: blinks LED with variable speed based on lux value */
void StartLedTask(void *argument)
{
  SensorData_t lastData = {0};
  uint32_t blinkDelay = 500;

  for(;;)
  {
    SensorData_t newData;
    if(osMessageQueueGet(sensorDataQueueHandle, &newData, NULL, 0) == osOK)
    {
      lastData = newData;
    }

    // Adjust blink speed: faster blink if bright, slower if dark
    if(lastData.lux > 5000)
      blinkDelay = 200;  // bright environment - fast blink
    else if(lastData.lux < 1000)
      blinkDelay = 800;  // dark environment - slow blink
    else
      blinkDelay = 500;  // moderate light

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    osDelay(blinkDelay);
  }
}

/* --- Peripheral init functions --- */
static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Configure PC13 (LED pin) as output */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

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
  HAL_I2C_Init(&hi2c1);
}

/* System clock config - use your STM32CubeMX generated one */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Placeholder error handler */
void Error_Handler(void)
{
  while(1)
  {
    // Optional: Can add a blink LED here to indicate error more clearly
  }
}
