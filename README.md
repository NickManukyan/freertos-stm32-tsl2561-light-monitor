# freertos-stm32-tsl2561-light-monitor

FreeRTOS-based embedded project for the STM32F103C8 microcontroller that reads ambient light intensity from a TSL2561 sensor via I2C and displays the lux value on a 16x2 I2C LCD. Moreover, it uses multitasking with FreeRTOS on the STM32.



## Setup

1. Install STM32CubeIDE
2. Clone this repository.
3. Import the project.
4. Add or implement the following drivers:
   - TSL2561 sensor driver (`tsl2561.h`/`c`)
   - I2C LCD driver (`lcd_i2c.h`/`c`)
5. Build and flash the project to the STM32 board.



## How It Works

- The **Sensor Task** reads the lux value from the TSL2561 sensor every 1 second and sends the data via a FreeRTOS message queue.
- The **Display Task** receives sensor data from the queue and updates the 16x2 LCD display with the current light intensity.
- The **LED Task** reads the latest sensor value and blinks the onboard LED at a variable rate: faster blinking for bright environments and slower for darker conditions.



## Hardware Required

- STM32F446RE NUCLEO Board
- TSL2561 Ambient Light Sensor
- I2C LCD Display (in this case 16x2)
- Connecting wires and breadboard

Optionally: You may connect a separate LED but in this setup the onboard LED is used. 



## Notes
- This project uses CMSIS-RTOS v2 APIs for FreeRTOS.
- The onboard LED is connected to PC13, which is active low.
- Ensure your I2C bus lines (SCL, SDA) have pull-up resistors if not included on modules.
- Requires TSL2561 driver (tsl2561.h/c) and I2C LCD driver (lcd_i2c.h/c) compatible with STM32 HAL.
- Uses FreeRTOS APIs (osThreadNew, osMessageQueueNew, etc.) with CMSIS-RTOS v2.

