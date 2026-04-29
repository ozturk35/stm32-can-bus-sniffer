#include "mcp2515.h"
#include "uart_console.h"
#include <string.h>

/* CNF register values for 8 MHz crystal.
 * Bit timing: 1 TQ sync + 2 TQ prop + 6 TQ phseg1 + 6 TQ phseg2 = 16 TQ */
typedef struct { uint8_t cnf1, cnf2, cnf3; uint16_t kbps; } baud_entry_t;

static const baud_entry_t s_baud_table[] = {
    { 0x01, 0xB1, 0x05, 125 },   /* 8 MHz crystal, 125 kbit/s */
    { 0x00, 0xB1, 0x05, 250 },   /* 8 MHz crystal, 250 kbit/s */
    { 0x00, 0x90, 0x02, 500 },   /* 8 MHz crystal, 500 kbit/s */
};

/* ---------- CS helpers --------------------------------------------------- */

static inline void cs_low(mcp2515_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_high(mcp2515_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/* ---------- Low-level register access ------------------------------------ */

uint8_t mcp2515_read_reg(mcp2515_t *dev, uint8_t addr)
{
    uint8_t tx[3] = { MCP_READ, addr, 0x00 };
    uint8_t rx[3] = { 0 };
    cs_low(dev);
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 3, HAL_MAX_DELAY);
    cs_high(dev);
    return rx[2];
}

void mcp2515_write_reg(mcp2515_t *dev, uint8_t addr, uint8_t val)
{
    uint8_t tx[3] = { MCP_WRITE, addr, val };
    uint8_t rx[3] = { 0 };
    cs_low(dev);
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 3, HAL_MAX_DELAY);
    cs_high(dev);
}

void mcp2515_bit_modify(mcp2515_t *dev, uint8_t addr, uint8_t mask, uint8_t data)
{
    uint8_t tx[4] = { MCP_BIT_MODIFY, addr, mask, data };
    uint8_t rx[4] = { 0 };
    cs_low(dev);
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 4, HAL_MAX_DELAY);
    cs_high(dev);
}

/* ---------- Reset -------------------------------------------------------- */

/* Send SPI RESET and poll until MCP2515 confirms config mode.
 * Uses TransmitReceive (same as reads) to keep SPI RX buffer clean.
 * Tight polling immediately after RESET catches the Config window; slow
 * polling continues up to 150 ms for cold-start crystals. */
static HAL_StatusTypeDef mcp2515_hw_reset(mcp2515_t *dev)
{
    uint8_t tx[1] = { MCP_RESET };
    uint8_t rx[1] = { 0 };
    cs_low(dev);
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 1, HAL_MAX_DELAY);
    cs_high(dev);

    /* Tight poll for first 10 ms — catches config mode within one SPI round-trip */
    uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < 10) {
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_CONFIG) {
            HAL_Delay(1);
            if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_CONFIG)
                return HAL_OK;
        }
    }

    /* Slow poll for remainder of 150 ms window */
    while (HAL_GetTick() - t0 < 150) {
        HAL_Delay(2);
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) != MCP_MODE_CONFIG)
            continue;
        HAL_Delay(2);
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_CONFIG)
            return HAL_OK;
    }
    return HAL_TIMEOUT;
}

/* ---------- Baud rate helper --------------------------------------------- */

static const baud_entry_t *find_baud(uint16_t kbps)
{
    for (size_t i = 0; i < sizeof(s_baud_table)/sizeof(s_baud_table[0]); i++) {
        if (s_baud_table[i].kbps == kbps)
            return &s_baud_table[i];
    }
    return NULL;
}

static void apply_baud(mcp2515_t *dev, const baud_entry_t *e)
{
    mcp2515_write_reg(dev, MCP_CNF3, e->cnf3);
    mcp2515_write_reg(dev, MCP_CNF2, e->cnf2);
    mcp2515_write_reg(dev, MCP_CNF1, e->cnf1);
}

/* ---------- Public API --------------------------------------------------- */

HAL_StatusTypeDef mcp2515_init(mcp2515_t *dev)
{
    /* Up to 5 reset attempts — CAN bus noise may cause occasional false timeouts */
    HAL_StatusTypeDef rst = HAL_TIMEOUT;
    for (int attempt = 0; attempt < 5 && rst != HAL_OK; attempt++)
        rst = mcp2515_hw_reset(dev);
    if (rst != HAL_OK) {
        uart_writef("FATAL: MCP2515 never reached config mode\r\n");
        return HAL_ERROR;
    }

    /* Configure 250 kbit/s */
    const baud_entry_t *e = find_baud(250);
    apply_baud(dev, e);
    dev->baud_kbps = 250;

    /* Verify CNF read-back */
    if (mcp2515_read_reg(dev, MCP_CNF1) != e->cnf1 ||
        mcp2515_read_reg(dev, MCP_CNF2) != e->cnf2 ||
        mcp2515_read_reg(dev, MCP_CNF3) != e->cnf3) {
        uart_writef("FATAL: MCP2515 CNF mismatch -- check VCC (5V module on 3.3V SPI?)\r\n");
        return HAL_ERROR;
    }

    /* Both RX buffers: receive any message (bypass filters) */
    mcp2515_write_reg(dev, MCP_RXB0CTRL, MCP_RXB_RXM_ANY);
    mcp2515_write_reg(dev, MCP_RXB1CTRL, MCP_RXB_RXM_ANY | 0x04);

    /* Enable RX interrupts for both buffers */
    mcp2515_write_reg(dev, MCP_CANINTE, MCP_INT_RX0 | MCP_INT_RX1);

    /* Enter normal mode — requires 11 recessive bits on RXCAN from CAN transceiver */
    mcp2515_write_reg(dev, MCP_CANCTRL, MCP_MODE_NORMAL);
    for (int i = 0; i < 500; i++) {
        HAL_Delay(1);
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_NORMAL)
            return HAL_OK;
    }
    uart_writef("FATAL: MCP2515 normal mode timeout -- check CAN transceiver and bus\r\n");
    return HAL_TIMEOUT;
}

void mcp2515_read_frame(mcp2515_t *dev, can_rx_frame_t *out)
{
    /* Check which buffer has data; prefer RXB0 */
    uint8_t intf = mcp2515_read_reg(dev, MCP_CANINTF);
    uint8_t cmd  = (intf & MCP_INT_RX0) ? MCP_READ_RX_BUF0 : MCP_READ_RX_BUF1;

    /* Burst-read: instruction + SIDH SIDL EID8 EID0 DLC D0..D7 = 14 bytes total */
    uint8_t tx[14] = { cmd, 0,0,0,0,0, 0,0,0,0,0,0,0,0 };
    uint8_t rx[14] = { 0 };
    cs_low(dev);
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 14, HAL_MAX_DELAY);
    cs_high(dev);

    /* rx[0] = dummy (cmd echo); rx[1]=SIDH rx[2]=SIDL rx[3]=EID8 rx[4]=EID0
     * rx[5]=DLC rx[6..13]=D0..D7 */
    uint8_t sidh = rx[1];
    uint8_t sidl = rx[2];
    uint8_t eid8 = rx[3];
    uint8_t eid0 = rx[4];
    uint8_t dlc  = rx[5] & 0x0F;

    int extended = (sidl >> 3) & 0x01;   /* IDE bit */
    uint32_t id;
    if (extended) {
        id = ((uint32_t)sidh << 21)
           | (((uint32_t)(sidl & 0xE0)) << 13)
           | (((uint32_t)(sidl & 0x03)) << 16)
           | ((uint32_t)eid8 << 8)
           |  (uint32_t)eid0;
        id |= MCP_EXT_FLAG;
    } else {
        id = ((uint32_t)sidh << 3) | ((sidl >> 5) & 0x07);
    }

    out->id  = id;
    out->dlc = dlc;
    memcpy(out->data, &rx[6], 8);

    /* Clear the interrupt flag for the buffer we read */
    uint8_t clr_mask = (cmd == MCP_READ_RX_BUF0) ? MCP_INT_RX0 : MCP_INT_RX1;
    mcp2515_bit_modify(dev, MCP_CANINTF, clr_mask, 0x00);
}

uint8_t mcp2515_read_eflg(mcp2515_t *dev)
{
    return mcp2515_read_reg(dev, MCP_EFLG);
}

HAL_StatusTypeDef mcp2515_set_baud(mcp2515_t *dev, uint16_t kbps)
{
    const baud_entry_t *e = find_baud(kbps);
    if (!e) return HAL_ERROR;

    /* Must be in config mode to change CNF registers */
    mcp2515_write_reg(dev, MCP_CANCTRL, MCP_MODE_CONFIG);
    for (int i = 0; i < 100; i++) {
        HAL_Delay(1);
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_CONFIG)
            goto in_config;
    }
    return HAL_TIMEOUT;

in_config:
    apply_baud(dev, e);
    dev->baud_kbps = kbps;
    mcp2515_write_reg(dev, MCP_CANCTRL, MCP_MODE_NORMAL);
    for (int i = 0; i < 200; i++) {
        HAL_Delay(1);
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_NORMAL)
            return HAL_OK;
    }
    return HAL_TIMEOUT;
}

void mcp2515_set_filter(mcp2515_t *dev, uint32_t id, uint32_t mask)
{
    /* Must configure in config mode */
    mcp2515_write_reg(dev, MCP_CANCTRL, MCP_MODE_CONFIG);
    for (int i = 0; i < 100; i++) {
        HAL_Delay(1);
        if ((mcp2515_read_reg(dev, MCP_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_CONFIG)
            break;
    }

    int ext = (id & MCP_EXT_FLAG) || (mask > 0x7FF);
    uint32_t raw_id   = id   & 0x1FFFFFFF;
    uint32_t raw_mask = mask & 0x1FFFFFFF;

    /* RXF0 (filter 0, applies to RXB0) */
    if (ext) {
        mcp2515_write_reg(dev, MCP_RXF0SIDH, (raw_id >> 21) & 0xFF);
        mcp2515_write_reg(dev, MCP_RXF0SIDL, (((raw_id >> 13) & 0xE0) | 0x08 | ((raw_id >> 16) & 0x03)));
        mcp2515_write_reg(dev, MCP_RXF0EID8, (raw_id >> 8) & 0xFF);
        mcp2515_write_reg(dev, MCP_RXF0EID0,  raw_id & 0xFF);
        /* RXM0 — extended mask */
        mcp2515_write_reg(dev, MCP_RXM0SIDH, (raw_mask >> 21) & 0xFF);
        mcp2515_write_reg(dev, MCP_RXM0SIDL, (((raw_mask >> 13) & 0xE0) | ((raw_mask >> 16) & 0x03)));
        mcp2515_write_reg(dev, MCP_RXM0EID8, (raw_mask >> 8) & 0xFF);
        mcp2515_write_reg(dev, MCP_RXM0EID0,  raw_mask & 0xFF);
        /* Apply filter to RXB0; RXB1 stays in any-receive mode */
        mcp2515_write_reg(dev, MCP_RXB0CTRL, 0x00);  /* RXM=00: filter active, extended */
    } else {
        /* Pass-all shortcut: set mask to 0 */
        mcp2515_write_reg(dev, MCP_RXB0CTRL, MCP_RXB_RXM_ANY);
        mcp2515_write_reg(dev, MCP_RXM0SIDH, 0x00);
        mcp2515_write_reg(dev, MCP_RXM0SIDL, 0x00);
        mcp2515_write_reg(dev, MCP_RXM0EID8, 0x00);
        mcp2515_write_reg(dev, MCP_RXM0EID0, 0x00);
    }

    mcp2515_write_reg(dev, MCP_CANCTRL, MCP_MODE_NORMAL);
}
