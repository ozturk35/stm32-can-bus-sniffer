#include "ring_buffer.h"
#include <string.h>

void ring_buf_init(ring_buf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

int ring_buf_is_empty(const ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

void ring_buf_write(ring_buf_t *rb, const can_rx_frame_t *f)
{
    uint32_t next = (rb->head + 1) & (RING_BUF_SIZE - 1);
    if (next == rb->tail)
        return;  /* full — caller already counted the drop */
    rb->buf[rb->head] = *f;
    rb->head = next;
}

int ring_buf_read(ring_buf_t *rb, can_rx_frame_t *out)
{
    if (rb->head == rb->tail)
        return 0;
    *out = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & (RING_BUF_SIZE - 1);
    return 1;
}
