/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "tsl2561.h"    //  TSL2561 driver
#include "lcd_i2c.h"    //  I2C LCD driver

/* Task handles --------------------------------------------------------------*/
osThreadId_t sensorTaskHandle;
osThreadId_t displayTaskHandle;
osThreadId_t ledTaskHandle;
osMutexId_t sensorDataMutexHandle;

/* I2C handle */
I2C_HandleTypeDef hi2c1;

/* Shared sensor data */
typedef struct {
    uint32_t lux;
} SensorData_t;

SensorData_t sharedSensorData = {0};

/* Function prototypes -------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);

void StartSensorTask(void *argument);
void StartDisplayTask(void *argument);
void StartLedTask(void *argument);

/* Main function --------------------------------------------------------------*/
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    /* Initialize peripherals */
    lcd_init(&hi2c1);
    tsl2561_init(&hi2c1);

    /* Create mutex */
    const osMutexAttr_t sensorDataMutex_attributes = {
        .name = "sensorDataMutex"
    };
    sensorDataMutexHandle = osMutexNew(&sensorDataMutex_attributes);

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

    /* Infinite loop */
    while (1) {}
}

/* Sensor Task: reads TSL2561 lux value every 1s */
void StartSensorTask(void *argument)
{
    SensorData_t tempData;
    for(;;)
    {
        if(tsl2561_read_lux(&tempData.lux) == HAL_OK)
        {
            osMutexAcquire(sensorDataMutexHandle, osWaitForever);
            sharedSensorData = tempData;
            osMutexRelease(sensorDataMutexHandle);
        }
        osDelay(1000);
    }
}

/* Display Task: updates LCD with latest lux value */
void StartDisplayTask(void *argument)
{
    char line1[17];
    SensorData_t localCopy;

    for(;;)
    {
        osMutexAcquire(sensorDataMutexHandle, osWaitForever);
        localCopy = sharedSensorData;
        osMutexRelease(sensorDataMutexHandle);

        snprintf(line1, sizeof(line1), "Light: %lu lux", localCopy.lux);
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print(line1);

        osDelay(1000);
    }
}

/* LED Task: blinks LED based on light intensity */
void StartLedTask(void *argument)
{
    SensorData_t localCopy;
    uint32_t blinkDelay = 500;

    for(;;)
    {
        osMutexAcquire(sensorDataMutexHandle, osWaitForever);
        localCopy = sharedSensorData;
        osMutexRelease(sensorDataMutexHandle);

        if(localCopy.lux > 5000)
            blinkDelay = 200;
        else if(localCopy.lux < 1000)
            blinkDelay = 800;
        else
            blinkDelay = 500;

        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        osDelay(blinkDelay);
    }
}

/* GPIO Init: PA5 for onboard LED */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* I2C1 Init: PB8=SCL, PB9=SDA */
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

/* System Clock Config: 8 MHz HSE → 72 MHz SYSCLK */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

/* Error Handler */
void Error_Handler(void)
{
    while(1)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
    }
}
