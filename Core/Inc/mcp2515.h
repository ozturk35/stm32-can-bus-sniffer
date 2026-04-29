#ifndef MCP2515_H
#define MCP2515_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* ---- SPI instructions --------------------------------------------------- */
#define MCP_RESET           0xC0
#define MCP_READ            0x03
#define MCP_WRITE           0x02
#define MCP_BIT_MODIFY      0x05
#define MCP_READ_STATUS     0xA0
#define MCP_READ_RX_BUF0    0x90   /* start at RXB0SIDH */
#define MCP_READ_RX_BUF1    0x94   /* start at RXB1SIDH */

/* ---- Register addresses ------------------------------------------------- */
#define MCP_RXF0SIDH    0x00
#define MCP_RXF0SIDL    0x01
#define MCP_RXF0EID8    0x02
#define MCP_RXF0EID0    0x03
#define MCP_RXF1SIDH    0x04
#define MCP_RXM0SIDH    0x20
#define MCP_RXM0SIDL    0x21
#define MCP_RXM0EID8    0x22
#define MCP_RXM0EID0    0x23
#define MCP_RXM1SIDH    0x24
#define MCP_RXM1SIDL    0x25
#define MCP_RXM1EID8    0x26
#define MCP_RXM1EID0    0x27
#define MCP_CNF3        0x28
#define MCP_CNF2        0x29
#define MCP_CNF1        0x2A
#define MCP_CANINTE     0x2B
#define MCP_CANINTF     0x2C
#define MCP_EFLG        0x2D
#define MCP_CANCTRL     0x0F
#define MCP_CANSTAT     0x0E
#define MCP_RXB0CTRL    0x60
#define MCP_RXB0SIDH    0x61
#define MCP_RXB0SIDL    0x62
#define MCP_RXB0EID8    0x63
#define MCP_RXB0EID0    0x64
#define MCP_RXB0DLC     0x65
#define MCP_RXB0D0      0x66
#define MCP_RXB1CTRL    0x70
#define MCP_RXB1SIDH    0x71
#define MCP_RXB1SIDL    0x72
#define MCP_RXB1EID8    0x73
#define MCP_RXB1EID0    0x74
#define MCP_RXB1DLC     0x75
#define MCP_RXB1D0      0x76

/* ---- Mode / control bits ------------------------------------------------ */
#define MCP_MODE_NORMAL     0x00
#define MCP_MODE_SLEEP      0x20
#define MCP_MODE_LOOPBACK   0x40
#define MCP_MODE_LISTENONLY 0x60
#define MCP_MODE_CONFIG     0x80
#define MCP_MODE_MASK       0xE0

/* ---- CANINTE / CANINTF bits --------------------------------------------- */
#define MCP_INT_RX0         0x01
#define MCP_INT_RX1         0x02
#define MCP_INT_TX0         0x04

/* ---- EFLG bits ---------------------------------------------------------- */
#define MCP_EFLG_RX1OVR     0x80
#define MCP_EFLG_RX0OVR     0x40
#define MCP_EFLG_TXBO       0x20
#define MCP_EFLG_TXEP       0x10
#define MCP_EFLG_RXEP       0x08

/* ---- RXBnCTRL ----------------------------------------------------------- */
#define MCP_RXB_RXM_ANY     0x60   /* receive any — bypass filters */

/* ---- Extended ID flag in our frame struct ------------------------------- */
#define MCP_EXT_FLAG        (1UL << 31)

/* ---- Data types --------------------------------------------------------- */

typedef struct {
    uint32_t timestamp_ms;
    uint32_t id;        /* bit 31 set = extended 29-bit frame */
    uint8_t  dlc;
    uint8_t  data[8];
} can_rx_frame_t;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint16_t           baud_kbps;   /* current configured baud rate */
} mcp2515_t;

/* ---- API ---------------------------------------------------------------- */

HAL_StatusTypeDef mcp2515_init(mcp2515_t *dev);
uint8_t           mcp2515_read_reg(mcp2515_t *dev, uint8_t addr);
void              mcp2515_write_reg(mcp2515_t *dev, uint8_t addr, uint8_t val);
void              mcp2515_bit_modify(mcp2515_t *dev, uint8_t addr, uint8_t mask, uint8_t data);
void              mcp2515_read_frame(mcp2515_t *dev, can_rx_frame_t *out);
uint8_t           mcp2515_read_eflg(mcp2515_t *dev);
HAL_StatusTypeDef mcp2515_set_baud(mcp2515_t *dev, uint16_t kbps);
void              mcp2515_set_filter(mcp2515_t *dev, uint32_t id, uint32_t mask);

#endif /* MCP2515_H */
