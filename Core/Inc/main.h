#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx_hal.h"

/* MCP2515 SPI1 wiring */
#define MCP_SPI_INSTANCE    SPI1
#define MCP_CS_PORT         GPIOB
#define MCP_CS_PIN          GPIO_PIN_6
#define MCP_INT_PORT        GPIOC
#define MCP_INT_PIN         GPIO_PIN_0

/* Board LEDs (Nucleo F446ZE) */
#define LD1_PORT            GPIOB
#define LD1_PIN             GPIO_PIN_0   /* green */
#define LD3_PORT            GPIOB
#define LD3_PIN             GPIO_PIN_14  /* red — used for error */

void SystemClock_Config(void);

#endif /* MAIN_H */
