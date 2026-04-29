#include <stdio.h>
#include <string.h>
#include "main.h"
#include "mcp2515.h"
#include "ring_buffer.h"
#include "uart_console.h"
#include "j1939_decode.h"

/* ---------- Globals (shared with stm32f4xx_it.c) ------------------------- */

mcp2515_t        g_mcp;
ring_buf_t       g_ring;
SPI_HandleTypeDef hspi1;

/* ---------- Clock: 180 MHz from HSE 8 MHz -------------------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 360;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;   /* 360/2 = 180 MHz */
    osc.PLL.PLLQ       = 7;
    HAL_RCC_OscConfig(&osc);

    /* Enable Over-Drive to reach 180 MHz */
    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK = 180 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;      /* APB1 = 45 MHz (USART3) */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;      /* APB2 = 90 MHz (SPI1)   */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);
}

/* ---------- GPIO init ---------------------------------------------------- */

static void gpio_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* PB6 — MCP2515 CS (output, start high) */
    g.Pin   = MCP_CS_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(MCP_CS_PORT, &g);
    HAL_GPIO_WritePin(MCP_CS_PORT, MCP_CS_PIN, GPIO_PIN_SET);

    /* PC0 — MCP2515 INT (input, falling-edge EXTI, pull-up) */
    g.Pin  = MCP_INT_PIN;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MCP_INT_PORT, &g);

    /* LD1 (green PB0), LD3 (red PB14) */
    g.Pin  = LD1_PIN | LD3_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LD1_PORT, &g);
    HAL_GPIO_WritePin(LD1_PORT, LD1_PIN | LD3_PIN, GPIO_PIN_RESET);

    /* EXTI0 priority 1 — enabled only when capture starts */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
}

/* ---------- SPI1 init ---------------------------------------------------- */

static void spi1_init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5 SCK, PA6 MISO, PA7 MOSI — AF5 */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    hspi1.Instance               = MCP_SPI_INSTANCE;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;    /* CPOL=0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;     /* CPHA=0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* 90/16 ≈ 5.6 MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
}

/* ---------- Error handler ------------------------------------------------ */

static void fatal(const char *msg)
{
    uart_writef("FATAL: %s\r\n", msg);
    HAL_GPIO_WritePin(LD3_PORT, LD3_PIN, GPIO_PIN_SET);   /* red LED on */
    for (;;) {}
}

/* ---------- main --------------------------------------------------------- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    SystemCoreClockUpdate();

    gpio_init();
    spi1_init();
    uart_console_init();
    ring_buf_init(&g_ring);

    /* MCP2515 init */
    g_mcp.hspi     = &hspi1;
    g_mcp.cs_port  = MCP_CS_PORT;
    g_mcp.cs_pin   = MCP_CS_PIN;

    if (mcp2515_init(&g_mcp) != HAL_OK)
        fatal("MCP2515 init failed -- check SPI wiring and VCC=3.3V");

    /* Boot banner */
    uint8_t canstat = mcp2515_read_reg(&g_mcp, MCP_CANSTAT);
    uart_writef("\r\nCAN Bus Sniffer -- ready\r\n"
                "STM32 Nucleo F446ZE | MCP2515 CANSTAT=0x%02X | %u kbit/s\r\n"
                "Type 'help' for commands.\r\n> ",
                canstat, (unsigned)g_mcp.baud_kbps);

    HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, GPIO_PIN_SET);   /* green LED on */

    /* Main loop */
    for (;;) {
        uart_process_input();

        /* Drain ring buffer and print decoded frames */
        can_rx_frame_t f;
        while (ring_buf_read(&g_ring, &f)) {
            char line[160];
            j1939_decode_and_print(&f, line, sizeof(line));
            uart_write(line);
            uart_write("\r\n");
        }
    }
}
