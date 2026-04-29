#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include "mcp2515.h"

#define RING_BUF_SIZE 256u   /* must be power of 2 */

typedef struct {
    can_rx_frame_t   buf[RING_BUF_SIZE];
    volatile uint32_t head;  /* written by ISR */
    volatile uint32_t tail;  /* read by main loop */
} ring_buf_t;

void ring_buf_init(ring_buf_t *rb);
int  ring_buf_is_empty(const ring_buf_t *rb);
/* write is called from ISR — caller manages overflow counting externally */
void ring_buf_write(ring_buf_t *rb, const can_rx_frame_t *f);
int  ring_buf_read(ring_buf_t *rb, can_rx_frame_t *out);

#endif /* RING_BUFFER_H */
