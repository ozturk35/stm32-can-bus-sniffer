#include "j1939_decode.h"
#include <stdio.h>
#include <string.h>

static void append_raw_bytes(char *out, size_t out_size, const can_rx_frame_t *f)
{
    /* Append " XX XX ..." for dlc bytes */
    size_t used = strlen(out);
    for (int i = 0; i < f->dlc && used + 4 < out_size; i++) {
        int n = snprintf(out + used, out_size - used, " %02X", f->data[i]);
        if (n > 0) used += (size_t)n;
    }
}

void j1939_decode_and_print(const can_rx_frame_t *f, char *out, size_t out_size)
{
    int      extended = (f->id & MCP_EXT_FLAG) != 0;
    uint32_t raw_id   = f->id & 0x1FFFFFFF;

    /* Timestamp and header */
    int n = snprintf(out, out_size, "[%7lu.%03lu] %s 0x%08lX [%u]",
        (unsigned long)(f->timestamp_ms / 1000),
        (unsigned long)(f->timestamp_ms % 1000),
        extended ? "29b" : "11b",
        (unsigned long)raw_id,
        (unsigned)f->dlc);
    if (n <= 0) return;

    append_raw_bytes(out, out_size, f);

    if (!extended) return;   /* no J1939 decode for standard frames */

    /* Extract J1939 fields */
    uint8_t  sa  = (uint8_t)(raw_id & 0xFF);
    uint8_t  pf  = (uint8_t)((raw_id >> 16) & 0xFF);
    uint32_t pgn = (raw_id >> 8) & 0x3FFFF;
    if (pf < 0xF0) pgn &= 0x3FF00U;   /* PDU1: mask out destination-address byte */

    size_t used = strlen(out);

    switch (pgn) {

    case 61444: {   /* EEC1 — Engine Speed */
        if (f->dlc >= 5) {
            uint16_t raw_rpm = (uint16_t)((f->data[4] << 8) | f->data[3]);
            /* 0.125 RPM/bit; multiply by 125 and divide by 1000 to avoid float */
            uint32_t rpm_x10 = (uint32_t)raw_rpm * 125u / 100u;
            snprintf(out + used, out_size - used,
                     "  PGN61444 EEC1 RPM=%lu.%lu SA=0x%02X",
                     (unsigned long)(rpm_x10 / 10),
                     (unsigned long)(rpm_x10 % 10),
                     (unsigned)sa);
        }
        break;
    }

    case 65262: {   /* Engine Temperature 1 — Coolant */
        if (f->dlc >= 1) {
            int coolant = (int)f->data[0] - 40;
            snprintf(out + used, out_size - used,
                     "  PGN65262 EngTemp Coolant=%dC SA=0x%02X",
                     coolant, (unsigned)sa);
        }
        break;
    }

    case 65265: {   /* CCVS — Vehicle Speed */
        if (f->dlc >= 2) {
            uint16_t raw_spd = (uint16_t)((f->data[1] << 8) | f->data[0]);
            /* 1/256 km/h per bit → raw * 10 / 256 gives tenths of km/h */
            uint32_t spd_x10 = (uint32_t)raw_spd * 10u / 256u;
            snprintf(out + used, out_size - used,
                     "  PGN65265 CCVS Speed=%lu.%lukm/h SA=0x%02X",
                     (unsigned long)(spd_x10 / 10),
                     (unsigned long)(spd_x10 % 10),
                     (unsigned)sa);
        }
        break;
    }

    case 65276: {   /* Dash Display — Fuel Level */
        if (f->dlc >= 2) {
            /* 0.4%/bit → raw * 4 / 10 gives tenths of percent */
            uint32_t fuel_x10 = (uint32_t)f->data[1] * 4u;
            snprintf(out + used, out_size - used,
                     "  PGN65276 Dash Fuel=%lu.%lu%% SA=0x%02X",
                     (unsigned long)(fuel_x10 / 10),
                     (unsigned long)(fuel_x10 % 10),
                     (unsigned)sa);
        }
        break;
    }

    default:
        /* Unknown PGN — raw bytes already printed, nothing to add */
        break;
    }
}
