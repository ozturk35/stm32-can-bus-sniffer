#ifndef J1939_DECODE_H
#define J1939_DECODE_H

#include <stddef.h>
#include "mcp2515.h"

/* Fills `out` with a null-terminated decoded line (no newline).
 * Format matches FSD Section 6.3. */
void j1939_decode_and_print(const can_rx_frame_t *f, char *out, size_t out_size);

#endif /* J1939_DECODE_H */
