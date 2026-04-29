#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "stm32f4xx_hal.h"
#include "uart_console.h"
#include "mcp2515.h"
#include "ring_buffer.h"

#define CON_LINE_MAX 128

UART_HandleTypeDef huart3;

/* Shared state — also accessed from stm32f4xx_it.c */
volatile int      g_state           = 0;   /* 0=IDLE, 1=CAPTURING */
volatile uint32_t g_frames_captured = 0;
volatile uint32_t g_frames_dropped  = 0;

extern mcp2515_t g_mcp;

static char s_line[CON_LINE_MAX];
static int  s_pos = 0;

/* ---------- Output helpers ----------------------------------------------- */

void uart_write(const char *s)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

void uart_writef(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uart_write(buf);
}

/* ---------- Init --------------------------------------------------------- */

void uart_console_init(void)
{
    /* USART3: PD8 TX (AF7), PD9 RX (AF7) — ST-Link VCP on Nucleo F446ZE */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &g);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

/* ---------- Command processing ------------------------------------------- */

static const char *eflg_desc(uint8_t e)
{
    if (e == 0) return "no errors";
    static char buf[48];
    buf[0] = '\0';
    if (e & MCP_EFLG_RX1OVR) strcat(buf, "RX1OVR ");
    if (e & MCP_EFLG_RX0OVR) strcat(buf, "RX0OVR ");
    if (e & MCP_EFLG_TXBO)   strcat(buf, "TXBO ");
    if (e & MCP_EFLG_TXEP)   strcat(buf, "TXEP ");
    if (e & MCP_EFLG_RXEP)   strcat(buf, "RXEP ");
    return buf;
}

static void cmd_status(void)
{
    uint8_t eflg = mcp2515_read_eflg(&g_mcp);

    /* Check and clear overflow flags */
    if (eflg & (MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR)) {
        uart_write("WARN: MCP2515 hardware RX overflow — clearing EFLG\r\n");
        mcp2515_bit_modify(&g_mcp, MCP_EFLG, MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR, 0x00);
    }

    uart_writef(
        "State:          %s\r\n"
        "Baud rate:      %u kbit/s\r\n"
        "Frames RX:      %lu\r\n"
        "Frames dropped: %lu\r\n"
        "MCP2515 EFLG:   0x%02X (%s)\r\n",
        g_state ? "CAPTURING" : "IDLE",
        (unsigned)g_mcp.baud_kbps,
        (unsigned long)g_frames_captured,
        (unsigned long)g_frames_dropped,
        eflg, eflg_desc(eflg)
    );
}

static void cmd_set_baud(const char *arg)
{
    if (!arg) { uart_write("ERROR: usage: set baud <125|250|500>\r\n"); return; }
    uint16_t kbps = (uint16_t)atoi(arg);
    if (kbps != 125 && kbps != 250 && kbps != 500) {
        uart_write("ERROR: supported baud rates: 125, 250, 500 kbit/s\r\n");
        return;
    }
    if (mcp2515_set_baud(&g_mcp, kbps) != HAL_OK) {
        uart_write("ERROR: baud rate change failed\r\n");
        return;
    }
    uart_writef("OK: baud set to %u kbit/s\r\n", kbps);
}

static void cmd_set_filter(const char *id_str, const char *mask_str)
{
    if (!id_str || !mask_str) {
        uart_write("ERROR: usage: set filter <id_hex> <mask_hex>\r\n");
        return;
    }
    uint32_t id   = (uint32_t)strtoul(id_str,   NULL, 16);
    uint32_t mask = (uint32_t)strtoul(mask_str,  NULL, 16);
    mcp2515_set_filter(&g_mcp, id, mask);
    uart_writef("OK: filter set (id=0x%08lX mask=0x%08lX)\r\n",
                (unsigned long)id, (unsigned long)mask);
}

static void process_line(char *line)
{
    /* Strip trailing whitespace */
    int len = (int)strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' || line[len-1] == ' '))
        line[--len] = '\0';

    if (len == 0) { uart_write("> "); return; }

    /* Lowercase in-place for case-insensitive matching */
    for (int i = 0; line[i]; i++)
        line[i] = (char)tolower((unsigned char)line[i]);

    if (strcmp(line, "start") == 0) {
        g_state = 1;
        HAL_NVIC_EnableIRQ(EXTI0_IRQn);
        uart_write("OK: capture started\r\n");

    } else if (strcmp(line, "stop") == 0) {
        HAL_NVIC_DisableIRQ(EXTI0_IRQn);
        g_state = 0;
        uart_write("OK: capture stopped\r\n");

    } else if (strncmp(line, "set baud ", 9) == 0) {
        cmd_set_baud(line + 9);

    } else if (strncmp(line, "set filter ", 11) == 0) {
        char *rest     = line + 11;
        char *id_str   = strtok(rest, " ");
        char *mask_str = strtok(NULL, " ");
        cmd_set_filter(id_str, mask_str);

    } else if (strcmp(line, "clear") == 0) {
        g_frames_captured = 0;
        g_frames_dropped  = 0;
        uart_write("OK: counters cleared\r\n");

    } else if (strcmp(line, "status") == 0) {
        cmd_status();

    } else if (strcmp(line, "help") == 0) {
        uart_write(
            "Commands:\r\n"
            "  start                        -- begin CAN capture\r\n"
            "  stop                         -- halt capture\r\n"
            "  set baud <125|250|500>       -- set bus baud rate (kbit/s)\r\n"
            "  set filter <id_hex> <mask>   -- set acceptance filter (29-bit IDs)\r\n"
            "  clear                        -- reset frame counters\r\n"
            "  status                       -- show state and statistics\r\n"
            "  help                         -- this message\r\n"
        );

    } else {
        uart_writef("ERROR: unknown command '%s' -- type 'help'\r\n", line);
    }

    uart_write("> ");
}

/* ---------- Main-loop input poller --------------------------------------- */

void uart_process_input(void)
{
    uint8_t ch;
    if (HAL_UART_Receive(&huart3, &ch, 1, 0) != HAL_OK)
        return;

    if (ch == '\r' || ch == '\n') {
        s_line[s_pos] = '\0';
        uart_write("\r\n");
        process_line(s_line);
        s_pos = 0;
    } else if ((ch == 0x7F || ch == 0x08) && s_pos > 0) {
        s_pos--;
        uart_write("\b \b");
    } else if (s_pos < CON_LINE_MAX - 1 && ch >= 0x20) {
        s_line[s_pos++] = (char)ch;
        HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);  /* echo */
    }
}
