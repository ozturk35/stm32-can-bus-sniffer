#include "stm32f4xx_hal.h"
#include "mcp2515.h"
#include "ring_buffer.h"

extern mcp2515_t         g_mcp;
extern ring_buf_t        g_ring;
extern volatile uint32_t g_frames_captured;
extern volatile uint32_t g_frames_dropped;
extern volatile int      g_state;

#define STATE_CAPTURING 1

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void EXTI0_IRQHandler(void)
{
    /* Clear pending bit before SPI read so a second arriving frame doesn't
     * re-assert before we return from the ISR */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);

    if (g_state != STATE_CAPTURING)
        return;

    can_rx_frame_t f;
    f.timestamp_ms = HAL_GetTick();
    mcp2515_read_frame(&g_mcp, &f);

    uint32_t next = (g_ring.head + 1) & (RING_BUF_SIZE - 1);
    if (next != g_ring.tail) {
        g_ring.buf[g_ring.head] = f;
        g_ring.head = next;
        g_frames_captured++;
    } else {
        g_frames_dropped++;
    }
}
