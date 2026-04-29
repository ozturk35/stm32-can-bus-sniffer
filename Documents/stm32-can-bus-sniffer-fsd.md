# STM32 CAN Bus Sniffer — Functional Specification Document (FSD)

**Version:** 1.1  
**Date:** 2026-04-30  
**Platform:** STM32 Nucleo F446ZE (SLOT4)  
**Status:** Phase 2 Complete

---

## 1. System Overview

### 1.1 Purpose

This document specifies the bare-metal STM32 HAL firmware for a CAN bus sniffer and live decoder running on the STM32 Nucleo F446ZE. The sniffer captures all CAN 2.0A/B frames on a shared bus, stores them in a ring buffer (zero frame loss under burst load), and outputs decoded SAE J1939 PGNs with timestamps to a UART terminal. A UART command interface allows the operator to control capture, configure filters, set baud rate, and inspect statistics.

### 1.2 Problem Statement

Verifying a CAN traffic generator (the companion ESP32-S3 node from Wish 01b) requires a passive observer that captures every frame, timestamps it, and presents human-readable decoded output. The STM32 Nucleo F446ZE provides this role: it is always a listener, never a transmitter, and must sustain capture at 250 kbit/s burst rates without frame loss.

### 1.3 Users and Stakeholders

| Stakeholder | Role |
|---|---|
| Firmware developer | Operates the sniffer terminal; validates generator output |
| ESP32-S3 generator (Wish 01b) | Produces the CAN traffic this sniffer consumes |

### 1.4 Goals

- Capture every CAN frame on the bus with a millisecond-resolution timestamp and zero frame loss under burst load.
- Decode SAE J1939 PGNs 61444, 65262, 65265, 65276 into human-readable values.
- Provide a UART command interface for filter configuration, baud rate selection, capture control, and statistics.
- Run bare-metal on STM32 HAL with no RTOS, using an interrupt-driven ring buffer.

### 1.5 Non-Goals

- CAN frame transmission (sniffer is receive-only).
- Full SAE J1939 stack (address claiming, transport protocol, diagnostics).
- Support for CAN-FD.
- Data logging to non-volatile storage (SD card, internal flash).
- Wireless or network output (UART terminal only).

### 1.6 High-Level System Flow

```
CAN Bus (250 kbit/s, J1939)
        │
        ▼
  MCP2515 SPI CAN Controller (SPI1, CS=PA4, INT=PC0)
        │
   EXTI0 interrupt on PC0 (falling edge)
        │
        ▼
  ISR — reads RX buffer via SPI, writes to ring buffer
        │
        ▼
  Main loop — drains ring buffer, J1939 decodes, prints to UART
        │
        ▼
  USART3 (ST-Link VCP, 115200 baud → /dev/ttyACM*)
        │
        ▼
  Developer Terminal
        │ (commands typed)
        ▼
  UART Command Interface (start, stop, set baud, set filter, clear, status)
```

---

## 2. System Architecture

### 2.1 Logical Architecture

The firmware uses a two-context design with no RTOS:

| Context | Responsibility |
|---|---|
| **EXTI0 ISR (PC0)** | Triggered on MCP2515 INT falling edge. Reads received frame from MCP2515 RX buffer via SPI. Writes `can_rx_frame_t` to ring buffer. Clears MCP2515 interrupt flag. |
| **Main loop** | Continuously drains the ring buffer. Decodes J1939 PGNs. Outputs decoded frames to UART. Polls USART3 for command characters (non-blocking). |

The ring buffer is the sole inter-context data path. It is a power-of-two circular buffer; the ISR writes to `head` and the main loop reads from `tail`. Critical section (`__disable_irq` / `__enable_irq`) is used only at the ISR side to update `head` atomically on non-atomic-capable architectures; on Cortex-M4 single-word writes are atomic, so the ISR needs no disable in practice, but the guard is retained for clarity.

Operating state machine:

```
IDLE ──start──► CAPTURING
CAPTURING ──stop──► IDLE
```

In IDLE state, MCP2515 RX interrupts are disabled. Incoming frames are not captured.

### 2.2 Hardware / Platform Architecture

#### STM32 Board

| Parameter | Value |
|---|---|
| MCU | STM32F446ZET6 |
| Architecture | Cortex-M4F, 180 MHz |
| Flash | 512 KB |
| RAM | 128 KB |
| Board | STM32 Nucleo F446ZE |
| Workbench slot | SLOT4 |
| Debug/Flash | ST-Link V2.1 (USB VID:PID `0483:374b`) |
| Console UART | USART3 via ST-Link VCP (`/dev/bench_nucleo` or `/dev/ttyACM*`) |

#### MCP2515 SPI CAN Controller — Wiring

| MCP2515 Pin | STM32 Pin | SPI1 Signal | Notes |
|---|---|---|---|
| VCC | 3.3V | — | Must be 3.3V — not 5V |
| GND | GND | — | |
| SCK | PA5 | SPI1_SCK | Arduino CN7 D13 |
| SI (MOSI) | PA7 | SPI1_MOSI | Arduino CN7 D11 |
| SO (MISO) | PA6 | SPI1_MISO | Arduino CN7 D12 |
| CS | PA4 | GPIO output | ZIO CN11; active-low; driven by firmware (SPI1_NSS AF5 not used — software CS) |
| INT | PC0 | EXTI0 input | Arduino CN9 A5; active-low; falling-edge interrupt |

#### CAN Bus Topology

| Node | Role | Termination |
|---|---|---|
| MCP2515 #1 (STM32 Nucleo — sniffer) | Receiver only | 120 Ω across CANH/CANL |
| MCP2515 #2 (ESP32-S3 — generator) | Transmitter | 120 Ω across CANH/CANL |

- Bus speed: 250 kbit/s (SAE J1939 standard).
- Wire length: short bench cable (< 50 cm); twisted pair preferred.
- Both nodes must have independent 120 Ω termination resistors.

### 2.3 Software Architecture

#### Build Environment

| Parameter | Value |
|---|---|
| Toolchain | arm-none-eabi-gcc (GCC 10+) |
| SDK | STM32 HAL (STM32CubeF4) |
| Build system | Makefile (bare-metal) |
| Linker script | STM32F446ZETx_FLASH.ld |
| Flash tool | OpenOCD (ST-Link) or st-flash |
| Standard library | newlib-nano |
| Compiler flags | `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os` |

#### Module Decomposition

```
Core/
  main.c               — System init, main loop, ring buffer drain, command dispatcher
  mcp2515.c/.h         — MCP2515 SPI driver: init, RX, baud rate config, filter config
  ring_buffer.c/.h     — Power-of-two circular buffer of can_rx_frame_t
  j1939_decode.c/.h    — J1939 PGN extractor, SPN decoders for four known PGNs
  uart_console.c/.h    — UART line reader, command parser, response formatter
Drivers/STM32F4xx_HAL_Driver/   — STM32 HAL (from STM32CubeF4 package)
```

#### Boot Sequence

1. `HAL_Init()`: SysTick for 1 ms HAL timebase; NVIC priority grouping.
2. System clock: 180 MHz from HSE + PLL (STM32F446ZE maximum).
3. GPIO init: PA4 as output (CS, driven high); PC0 as input (EXTI0, falling-edge, pull-up enabled).
4. SPI1 init: Master, CPOL=0/CPHA=0, 8-bit, prescaler 128 → ~703 kHz (APB2 90 MHz ÷ 128). Conservative rate chosen for noise tolerance on short bench wiring.
5. USART3 init: 115200, 8N1 (PD8 TX, PD9 RX via ST-Link VCP).
6. Ring buffer init: 256 entries, head = tail = 0.
7. MCP2515 driver init: reset → read CANSTAT (expect 0x80) → write CNF1/CNF2/CNF3 for 250 kbit/s → read-back verify → configure RXB0/RXB1 → write acceptance mask (pass-all default) → set normal mode → enable RX0IE + RX1IE in CANINTE.
8. If step 7 fails: print fatal error on USART3, halt in `while(1)` error loop.
9. Print boot banner on USART3.
10. Enter main loop in IDLE state (EXTI0 NVIC disabled).
11. On `start` command: enable EXTI0 in NVIC; transition to CAPTURING state.

---

## 3. Implementation Phases

### 3.1 Phase 1 — Infrastructure Foundation

**Scope**

- SPI1 GPIO and peripheral initialisation.
- MCP2515 low-level driver: reset, register read/write, CNF baud rate config, normal mode entry.
- Polling RX: read a single received frame from MCP2515 RXB0 by polling CANINTF.RX0IF (no interrupt yet).
- USART3 console: init, print boot banner, poll-read single characters, echo, line assembly.
- Verify SPI comms by reading CANSTAT and verifying CNF register read-back.

**Deliverables**

- `mcp2515.c/.h`: `mcp2515_init()`, `mcp2515_read_reg()`, `mcp2515_write_reg()`, `mcp2515_read_frame()`.
- `uart_console.c/.h`: line input, command echo, boot banner.
- `ring_buffer.c/.h`: circular buffer with producer/consumer API.
- Boot banner printed on USART3 after successful MCP2515 init.

**Exit Criteria**

- TC-1.1, TC-1.2, TC-1.3 pass.
- A CAN frame from the ESP32-S3 generator is received by polling, and raw bytes are printed on USART3 within 1 second of `start`.
- No SPI errors or MCP2515 error flags after 60 seconds of idle bus.

**Dependencies**

- MCP2515 module wired to Nucleo F446ZE per pin table in Section 2.2.
- ESP32-S3 generator (Wish 01b) operational and transmitting at 250 kbit/s.

---

### 3.2 Phase 2 — Ring Buffer, Interrupt-Driven RX, J1939 Decode, Full Command Interface

**Scope**

- Interrupt-driven RX: EXTI0 ISR on PC0 reads MCP2515 RX buffer, writes to ring buffer.
- Support for MCP2515 RXB1 (second receive buffer) to enable back-to-back frame hardware buffering.
- Ring buffer consumer in main loop: drain buffer, decode J1939 PGNs, print formatted output.
- J1939 decoder: PGN extraction from 29-bit ID, SPN decoding for all four target PGNs.
- Full UART command interface: `start`, `stop`, `set baud`, `set filter`, `clear`, `status`, `help`.
- Drop counter: frames lost when ring buffer is full.

**Deliverables**

- `j1939_decode.c/.h`: `j1939_extract_pgn()`, individual SPN decoders for PGNs 61444, 65262, 65265, 65276.
- Complete UART command handler with all commands implemented.
- `status` command output: state, frame count, drop count, EFLG, baud rate.
- Live UART terminal output with timestamp, ID, DLC, raw hex, decoded PGN tag and values.

**Exit Criteria**

- TC-2.1 through TC-2.6 pass.
- TC-3.1 through TC-3.3 pass.
- TC-4.1 through TC-4.5 pass.
- TC-5.1 (burst stress test — `Frames dropped: 0` after 60 s burst) passes.
- All four PGNs decoded with values matching generator output.

**Dependencies**

- Phase 1 complete and exit criteria met.

---

## 4. Functional Requirements

### 4.1 Functional Requirements (FR)

#### FR-1: MCP2515 Initialisation

| ID | Priority | Requirement |
|---|---|---|
| FR-1.1 | Must | The firmware shall initialise the MCP2515 over SPI1 at startup using the pin assignment in Section 2.2. |
| FR-1.2 | Must | The firmware shall configure the MCP2515 for 250 kbit/s operation using CNF1/CNF2/CNF3 values for an 8 MHz crystal (see Appendix A.2). |
| FR-1.3 | Must | The firmware shall set the MCP2515 to normal operating mode before enabling receive interrupts. |
| FR-1.4 | Must | If MCP2515 initialisation fails (CANSTAT not 0x80 after reset, or CNF register read-back mismatch), the firmware shall log a fatal error on USART3 and halt in an error loop. |
| FR-1.5 | Should | The firmware shall print a boot banner on USART3 after successful init, including the CANSTAT value and configured baud rate. |

#### FR-2: CAN Frame Capture

| ID | Priority | Requirement |
|---|---|---|
| FR-2.1 | Must | The firmware shall capture all CAN frames (CAN 2.0A standard 11-bit and CAN 2.0B extended 29-bit) received by the MCP2515 while in CAPTURING state. |
| FR-2.2 | Must | Frame capture shall be interrupt-driven: the MCP2515 INT pin (PC0) falling edge shall trigger EXTI0, which reads the received frame from the MCP2515 RX buffer via SPI and writes it to the ring buffer. |
| FR-2.3 | Must | Each captured frame shall be tagged with a millisecond-resolution SysTick timestamp at the moment of ISR entry. |
| FR-2.4 | Must | The firmware shall enable both MCP2515 receive buffers (RXB0 and RXB1) so that the hardware can hold two back-to-back frames before the ISR services the interrupt. |
| FR-2.5 | Must | No frame shall be dropped due to ring buffer overflow under the generator's burst mode (maximum bus load at 250 kbit/s with 8-byte frames). |
| FR-2.6 | Should | If the ring buffer is full when the ISR attempts to write, the firmware shall increment a dropped-frame counter and discard the frame without blocking or crashing. |

#### FR-3: J1939 Decoding

| ID | Priority | Requirement |
|---|---|---|
| FR-3.1 | Must | The firmware shall extract the J1939 PGN, priority, and source address (SA) from any 29-bit extended CAN ID. |
| FR-3.2 | Must | The firmware shall decode PGN 61444 (EEC1): extract SPN 190 (bytes 3–4, 16-bit LE, 0.125 RPM/bit) and display value in RPM with one decimal place. |
| FR-3.3 | Must | The firmware shall decode PGN 65262 (Engine Temperature 1): extract SPN 110 (byte 0, 1°C/bit, offset −40°C) and display value in °C. |
| FR-3.4 | Must | The firmware shall decode PGN 65265 (CCVS): extract SPN 84 (bytes 0–1, 16-bit LE, 1/256 km/h per bit) and display value in km/h with one decimal place. |
| FR-3.5 | Must | The firmware shall decode PGN 65276 (Dash Display): extract SPN 96 (byte 1, 0.4%/bit) and display value as a percentage with one decimal place. |
| FR-3.6 | Should | For all other CAN IDs (unknown PGN or standard 11-bit frames), the firmware shall print the raw frame (ID, DLC, payload bytes) without a decode tag. |

#### FR-4: UART Terminal Output

| ID | Priority | Requirement |
|---|---|---|
| FR-4.1 | Must | Each frame shall be printed on USART3 as a single line containing: timestamp in ms, frame format (11b/29b), CAN ID in hex, DLC, raw payload bytes in hex, and J1939 decode string if the PGN is known. |
| FR-4.2 | Must | UART output shall be performed by the main loop only — the ISR shall not call any UART output function. |
| FR-4.3 | Should | The output line format shall follow the example in Section 6.3. |

#### FR-5: UART Command Interface

| ID | Priority | Requirement |
|---|---|---|
| FR-5.1 | Must | The firmware shall provide a command interface on USART3 at 115200 baud, 8N1. |
| FR-5.2 | Must | The `start` command shall enable EXTI0 in NVIC and transition to CAPTURING state. |
| FR-5.3 | Must | The `stop` command shall disable EXTI0 in NVIC and transition to IDLE state. |
| FR-5.4 | Must | The `set baud <rate>` command shall reconfigure the MCP2515 CNF registers and re-enter normal mode. Supported rates: 125, 250, 500 (kbit/s). |
| FR-5.5 | Must | The `set filter <id_hex> <mask_hex>` command shall configure MCP2515 acceptance filter 0 and mask 0 for 29-bit extended IDs. A mask of `00000000` accepts all frames. |
| FR-5.6 | Must | The `clear` command shall reset all frame and drop counters to zero. |
| FR-5.7 | Must | The `status` command shall print: capture state, baud rate, total frames captured, frames dropped, and MCP2515 EFLG register value. |
| FR-5.8 | Should | The `help` command shall print a one-line summary of all available commands. |
| FR-5.9 | Should | On receipt of an unrecognised command, the firmware shall print an error message and hint to type `help`. |

#### FR-6: Error Handling

| ID | Priority | Requirement |
|---|---|---|
| FR-6.1 | Should | The firmware shall read the MCP2515 EFLG register when the `status` command is issued and display any active error flags. |
| FR-6.2 | Should | On detection of MCP2515 RX buffer overflow flags (RX0OVR or RX1OVR in EFLG), the firmware shall log a warning on USART3 and clear the overflow flags. |
| FR-6.3 | May | The firmware shall detect a bus-off condition (TXBO in EFLG) and log a warning to USART3; since the sniffer does not transmit, bus-off is not expected under normal operation. |

---

### 4.2 Non-Functional Requirements (NFR)

| ID | Priority | Requirement |
|---|---|---|
| NFR-1.1 | Must | The firmware shall sustain reception of all CAN frames at 250 kbit/s with 8-byte payloads under generator burst mode (theoretical maximum ~3125 frames/sec at 100% bus load) without ring buffer overflow. |
| NFR-1.2 | Must | The EXTI0 ISR shall complete its SPI frame read and ring buffer write within one CAN frame time at 250 kbit/s with 8 bytes (~320 µs) so that the MCP2515 hardware buffers are drained before they overflow. |
| NFR-2.1 | Must | The firmware shall be built targeting Cortex-M4 with hardware FPU (`-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard`). |
| NFR-2.2 | Must | SPI1 shall be configured at a rate that keeps a 14-byte frame SPI transfer within one CAN frame time at the configured baud rate. At 250 kbit/s (320 µs/frame) and the implemented 703 kHz SPI rate, a 14-byte transfer takes ~159 µs — within budget. |
| NFR-3.1 | Should | Main loop drain rate at nominal simulation load (six PGN frames per second) shall not cause UART output backpressure on the ring buffer. |
| NFR-3.2 | Should | UART command response latency shall be < 100 ms from newline receipt. |
| NFR-4.1 | Should | Firmware shall operate continuously for ≥ 24 hours without crash, hard fault, or dropped frames under nominal simulation load. |

---

### 4.3 Constraints

| ID | Constraint |
|---|---|
| C-1 | MCP2515 VCC must be powered from 3.3V to match STM32 GPIO logic levels. 5V will damage the GPIO pins. |
| C-2 | A 120 Ω termination resistor must be present at each end of the CAN bus. |
| C-3 | SPI1 pins PA4/PA5/PA6/PA7 must not share the bus with any other SPI device. |
| C-4 | PC0 must be the sole EXTI0 source. STM32F4 EXTI lines are per-line number, not per-GPIO port — only one GPIO on EXTI0 can be active at a time. |
| C-5 | The MCP2515 crystal frequency is assumed to be 8 MHz. CNF register values must be recalculated if a different crystal is fitted. |
| C-6 | Only one terminal may be connected to the USART3 ST-Link VCP at a time. |

---

## 5. Risks, Assumptions and Dependencies

| # | Type | Description | Likelihood | Impact | Mitigation |
|---|---|---|---|---|---|
| R-1 | Technical Risk | MCP2515 CNF values depend on crystal frequency. An unlabelled 16 MHz crystal gives wrong baud rate. | Medium | High | Verify crystal by oscilloscope at power-up or compare sniffer IDs against generator injected IDs; wrong baud produces garbled/absent frames. |
| R-2 | Technical Risk | ISR SPI read time may exceed back-to-back frame arrival time at burst rate, overflowing MCP2515 hardware buffers before the ISR can drain them. | Low | Medium | MCP2515 holds 2 frames in RXB0+RXB1 (FR-2.4); at 250 kbit/s an 8-byte frame takes ~320 µs; ISR SPI read of 14 bytes at 703 kHz takes ~159 µs — within budget. Increasing SPI prescaler to 16 (5.6 MHz, ~20 µs transfer) is available if headroom is needed. |
| R-3 | Technical Risk | Main loop UART printing may fall behind ring buffer fill rate under sustained burst load (UART at 115200 baud limits to ~11,520 chars/sec; each output line ~100 chars → ~115 lines/sec, which is less than the burst frame rate). | Medium | Medium | During burst, printed output will lag behind ring buffer fill; ring buffer sized for 256 entries (~82 ms at burst rate). Add a `verbose` on/off flag in Phase 2 to suppress per-frame output during burst. |
| R-4 | Technical Risk | USART3 ST-Link VCP may not enumerate on all USB cables/hubs. | Low | Low | Use direct USB connection to the workbench machine. |
| R-5 | Dependency | ESP32-S3 generator (Wish 01b) must be operational to validate end-to-end capture. | — | — | Phase 1 exit criteria are met with a single generator inject frame; full burst test deferred to Phase 2. |

**Assumptions**

- MCP2515 module uses an 8 MHz crystal. (assumed)
- MCP2515 INT output is active-low open-drain with pull-up resistor on the module PCB. (assumed)
- STM32 HAL SPI master polling mode is used inside the ISR for simplicity and deterministic timing. (assumed)
- USART3 (PD8/PD9) is the ST-Link VCP UART on the Nucleo F446ZE. (assumed)
- The generator node has independent 120 Ω termination on its MCP2515 module. (assumed)

---

## 6. Interface Specifications

### 6.1 External Interfaces

#### CAN Bus (J1939 Physical Layer)

| Parameter | Value |
|---|---|
| Standard | ISO 11898-2 (CAN high-speed) |
| Baud rate | 250 kbit/s default; runtime-configurable via `set baud` |
| Frame format | CAN 2.0A (11-bit) and CAN 2.0B (29-bit extended) |
| Termination | 120 Ω at each node |
| Topology | Two-node bus (generator + sniffer) |

#### USART3 Console (ST-Link VCP)

| Parameter | Value |
|---|---|
| Interface | USART3 (PD8 TX, PD9 RX) via ST-Link VCP |
| Baud rate | 115200 |
| Frame format | 8N1 |
| Line ending | `\r\n` (output) / `\n` or `\r` accepted (input) |
| Workbench device | `/dev/bench_nucleo` or `/dev/ttyACM*` |

---

### 6.2 Internal Interfaces

| Interface | Type | Description |
|---|---|---|
| Ring buffer | Circular buffer (power-of-two, 256 entries) | ISR writes `can_rx_frame_t`; main loop reads. Head updated by ISR; tail updated by main loop. |
| Capture state | `volatile enum` global | `STATE_IDLE` / `STATE_CAPTURING` — set by command handler, checked by ISR gate. |
| Frame counters | `volatile uint32_t` globals | `g_frames_captured`, `g_frames_dropped` — incremented by ISR; read by command handler. |

---

### 6.3 Data Models

#### `can_rx_frame_t` (ring buffer element)

```c
typedef struct {
    uint32_t timestamp_ms;  /* SysTick-based timestamp at ISR entry */
    uint32_t id;            /* CAN ID; bit 31 set = extended 29-bit frame */
    uint8_t  dlc;           /* Data length code (0–8) */
    uint8_t  data[8];       /* Payload bytes */
} can_rx_frame_t;
```

#### Ring Buffer

```c
#define RING_BUF_SIZE 256   /* must be power of 2 */

typedef struct {
    can_rx_frame_t buf[RING_BUF_SIZE];
    volatile uint32_t head;  /* written by ISR */
    volatile uint32_t tail;  /* read by main loop */
} ring_buf_t;
```

Producer (ISR):
```c
uint32_t next = (rb->head + 1) & (RING_BUF_SIZE - 1);
if (next != rb->tail) {
    rb->buf[rb->head] = frame;
    rb->head = next;
    g_frames_captured++;
} else {
    g_frames_dropped++;
}
```

Consumer (main loop):
```c
while (rb->tail != rb->head) {
    can_rx_frame_t f = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & (RING_BUF_SIZE - 1);
    j1939_decode_and_print(&f);
}
```

#### Decoded Output Line Format

One line per frame on USART3:

```
[  1234.100] 29b 0x0CF00400 [8] 00 7D 7D 14 00 FF 0F 7D  PGN61444 EEC1 RPM=850.0
[  1234.200] 29b 0x0CF00400 [8] 00 7D 7D 1C 00 FF 0F 7D  PGN61444 EEC1 RPM=851.9
[  2234.512] 29b 0x18FEEE00 [8] 73 FF FF FF FF FF FF FF  PGN65262 EngTemp Coolant=75C
[  2234.513] 29b 0x18FEF100 [8] 00 00 FF FF FF FF FF FF  PGN65265 CCVS Speed=0.0km/h
[  2234.514] 29b 0x18FEFC00 [8] FF 4B FF FF FF FF FF FF  PGN65276 Dash Fuel=30.0%
[  2234.515] 11b 0x00000123 [4] DE AD BE EF
```

Fields (in order): `[timestamp_ms]`, frame format (`11b`/`29b`), CAN ID (8 hex digits), `[DLC]`, payload bytes (space-separated hex), decoded tag (if PGN is known).

---

### 6.4 Commands

All commands are ASCII, terminated by `\n` or `\r\n`. The parser shall be case-insensitive for command keywords.

| Command | Syntax | Description | Response on Success |
|---|---|---|---|
| `start` | `start` | Enable RX interrupts; begin capture (IDLE → CAPTURING) | `OK: capture started` |
| `stop` | `stop` | Disable RX interrupts; halt capture (CAPTURING → IDLE) | `OK: capture stopped` |
| `set baud` | `set baud <rate>` | Reconfigure MCP2515 for 125, 250, or 500 kbit/s | `OK: baud set to 250 kbit/s` |
| `set filter` | `set filter <id_hex> <mask_hex>` | Configure MCP2515 acceptance filter 0 and mask 0 (29-bit IDs). `mask=00000000` accepts all. | `OK: filter set` |
| `clear` | `clear` | Reset frame and drop counters to zero | `OK: counters cleared` |
| `status` | `status` | Print state, baud, frame count, drop count, EFLG | See §6.4.1 |
| `help` | `help` | Print command summary | Command list |

#### 6.4.1 `status` Command Output Format

```
State:          CAPTURING
Baud rate:      250 kbit/s
Frames RX:      14823
Frames dropped: 0
MCP2515 EFLG:   0x00 (no errors)
```

#### 6.4.2 `set filter` Examples

```
set filter 0CF00400 1FFFFFFF    # capture only PGN 61444 exact match
set filter 18FE0000 1FFE0000    # capture all PF=0xFE PGNs (65024–65279)
set filter 00000000 00000000    # promiscuous — accept all frames
```

---

## 7. Operational Procedures

### 7.1 Hardware Setup

1. Wire MCP2515 module to Nucleo F446ZE per pin table in Section 2.2. Measure VCC with a meter — must be 3.3V before powering on.
2. Connect CANH and CANL between the STM32 MCP2515 and the ESP32-S3 MCP2515.
3. Verify 120 Ω termination resistors are present at both ends. With both nodes connected and powered off, resistance across CANH/CANL should be ~60 Ω.
4. Connect Nucleo F446ZE to the workbench machine via USB. ST-Link enumerates as `/dev/bench_nucleo` (udev symlink) or `/dev/ttyACM*`.

### 7.2 Build and Flash

```bash
# Build (from firmware project root)
make -j8

# Flash via OpenOCD (ST-Link, auto-detected)
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/can_sniffer.elf verify reset exit"

# Alternatively via st-flash
st-flash --reset write build/can_sniffer.bin 0x8000000

# Open serial terminal
screen /dev/bench_nucleo 115200
# or
minicom -b 115200 -D /dev/bench_nucleo
```

### 7.3 Normal Operation

1. After boot, USART3 prints the banner and prompt (`>`).
2. Type `start` to begin capture.
3. Observe decoded PGN output — all four PGNs from the generator should appear within 2 seconds.
4. Type `status` to verify counters and check for dropped frames.
5. Type `stop` to halt capture.

### 7.4 Burst Stress Test

1. Type `start` on the sniffer.
2. On the ESP32-S3 generator terminal: `burst on`.
3. Observe sniffer terminal — frames flood at maximum rate.
4. After 60 seconds, type `status` on the sniffer — verify `Frames dropped: 0`.
5. On generator: `burst off`.

### 7.5 Filter Configuration

```
> set filter 0CF00400 1FFFFFFF
OK: filter set
> start
OK: capture started
```

Only PGN 61444 (engine speed) frames will appear. All other PGNs are filtered at the MCP2515 hardware level.

### 7.6 Baud Rate Change

```
> stop
OK: capture stopped
> set baud 500
OK: baud set to 500 kbit/s
> start
OK: capture started
```

> Note: both nodes must be at the same baud rate. Set the generator to the new rate before starting capture.

### 7.7 Recovery Procedures

| Scenario | Recovery Action |
|---|---|
| MCP2515 init failure on boot | Check SPI wiring and VCC voltage. Power-cycle the module. Reflash firmware. |
| No frames after `start` | Run `status` — check EFLG. Verify bus wiring. Verify generator is transmitting. Run `set filter 00000000 00000000` to clear any leftover filter. |
| Garbled characters on terminal | Verify host terminal baud is 115200. Nucleo VCP baud is fixed. |
| `Frames dropped > 0` | Reduce UART output verbosity; increase ring buffer size; check NVIC priority of EXTI0. |
| EFLG shows RX0OVR / RX1OVR | ISR is not draining MCP2515 fast enough — check for higher-priority ISRs blocking EXTI0. |
| Workbench shows SLOT4 absent | Replug Nucleo; wait for hotplug event at `localhost:8080`. |
| Firmware hard fault | Connect ST-Link debugger; read SCB->CFSR and SCB->MMFAR; check ring buffer bounds and stack usage. |

---

## 8. Verification and Validation

### 8.1 Phase 1 Verification

| Test ID | Feature | Procedure | Success Criteria |
|---|---|---|---|
| TC-1.1 | MCP2515 SPI comms | Read CANSTAT register immediately after reset. | Returns 0x80 (configuration mode). |
| TC-1.2 | 250 kbit/s config | Read CNF1, CNF2, CNF3 registers after init. | CNF1=0x00, CNF2=0xB1, CNF3=0x05 (see Appendix A.2). |
| TC-1.3 | Normal mode entry | Read CANSTAT OPMOD bits after init. | OPMOD = 0b000 (bits 7:5 of CANSTAT = 0x00). |
| TC-1.4 | Polling RX | Generator issues `inject 0CF00400 0000FF1400FF0F7D`; observe USART3. | STM32 terminal prints one raw frame line with ID 0x0CF00400 within 1 second. |
| TC-1.5 | Boot banner | Power-cycle Nucleo; observe USART3. | Banner printed within 2 seconds; includes CANSTAT value and baud rate. |

---

### 8.2 Phase 2 Verification

| Test ID | Feature | Procedure | Success Criteria |
|---|---|---|---|
| TC-2.1 | Interrupt-driven RX | Start generator simulation; type `start` on sniffer; observe output. | All four PGNs appear continuously without polling delay. |
| TC-2.2 | PGN 61444 decode | Observe terminal for PGN 61444 lines for 30 s. | RPM printed with correct formula; value in range 750–2200 RPM; value changes over time. |
| TC-2.3 | PGN 65262 decode | Observe terminal for PGN 65262 lines. | Coolant temperature in °C; range 75–95°C; formula `raw − 40` correct. |
| TC-2.4 | PGN 65265 decode | Observe terminal for PGN 65265 lines. | Vehicle speed in km/h; formula `raw / 256.0` correct; range 0–120 km/h. |
| TC-2.5 | PGN 65276 decode | Observe terminal for PGN 65276 lines. | Fuel level in %; formula `raw × 0.4` correct; range 30–95%. |
| TC-2.6 | All PGNs present | Run simulation for 10 s; collect terminal output. | All four PGNs (61444, 65262, 65265, 65276) decoded with correct values and IDs. |
| TC-3.1 | Ring buffer — nominal | Run simulation for 60 s; check `status`. | `Frames dropped: 0`; total frames ≈ expected rate × 60 s. |
| TC-3.2 | Ring buffer — burst | Generator `burst on` for 60 s; check `status`. | `Frames dropped: 0`; firmware remains responsive to UART commands. |
| TC-3.3 | Drop counter increment | Pause main loop via debugger; let ISR run for > 256 frames; resume; check `status`. | `Frames dropped` > 0; firmware does not crash or corrupt ring buffer. |
| TC-4.1 | `start` / `stop` | Type `start`, wait 2 s, type `stop`; observe output. | Frames appear only during CAPTURING window; correct `OK:` responses both times. |
| TC-4.2 | `set baud 250` | Type `set baud 250`; verify generator still at 250 kbit/s; observe frames. | `OK: baud set to 250 kbit/s`; frames continue appearing with correct decode. |
| TC-4.3 | `set filter` — exact | Type `set filter 0CF00400 1FFFFFFF`; `start`; observe terminal. | Only PGN 61444 frames appear; PGNs 65262, 65265, 65276 are absent. |
| TC-4.4 | `set filter` — all | Type `set filter 00000000 00000000`; `start`. | All four PGNs appear in output. |
| TC-4.5 | `clear` command | Run for 10 s; type `clear`; type `status`. | Counters are zero; capture continues without interruption. |
| TC-5.1 | End-to-end decode | Run simulation 60 s; compare sniffer counters vs generator `status`. | All four PGNs decoded correctly; sniffer frame count matches generator TX count within ±1%. |

---

### 8.3 Acceptance Tests

| Test ID | Scenario | Procedure | Success Criteria |
|---|---|---|---|
| AT-1 | Full simulation run | Start capture; run generator simulation for 5 minutes. | All four PGNs decoded continuously; `Frames dropped: 0`; decoded values in expected ranges; no hard fault. |
| AT-2 | Burst stress test | Generator `burst on` for 60 seconds; monitor sniffer. | `Frames dropped: 0`; firmware responsive to UART commands during burst; no hard fault. |
| AT-3 | 24-hour stability | Run simulation for 24 hours. | No crash; no dropped frames under nominal load; frame counters still incrementing at end of window. |

---

### 8.4 Traceability Matrix

| Requirement | Priority | Test Case(s) | Status |
|---|---|---|---|
| FR-1.1 | Must | TC-1.1 | Covered |
| FR-1.2 | Must | TC-1.2 | Covered |
| FR-1.3 | Must | TC-1.3 | Covered |
| FR-1.4 | Must | TC-1.5 (error path observed at power-up) | Covered |
| FR-1.5 | Should | TC-1.5 | Covered |
| FR-2.1 | Must | TC-2.1, TC-2.6 | Covered |
| FR-2.2 | Must | TC-2.1 | Covered |
| FR-2.3 | Must | TC-2.1 (timestamp visible in output) | Covered |
| FR-2.4 | Must | TC-3.2 (burst — both RX buffers exercised) | Covered |
| FR-2.5 | Must | TC-3.2, AT-2 | Covered |
| FR-2.6 | Should | TC-3.3 | Covered |
| FR-3.1 | Must | TC-2.6 (all PGN IDs correctly identified) | Covered |
| FR-3.2 | Must | TC-2.2 | Covered |
| FR-3.3 | Must | TC-2.3 | Covered |
| FR-3.4 | Must | TC-2.4 | Covered |
| FR-3.5 | Must | TC-2.5 | Covered |
| FR-3.6 | Should | TC-2.6 (non-J1939 raw line verified offline) | Covered |
| FR-4.1 | Must | TC-2.6 (output format checked) | Covered |
| FR-4.2 | Must | TC-2.1 (ISR vs main loop separation confirmed) | Covered |
| FR-4.3 | Should | TC-2.6 | Covered |
| FR-5.1 | Must | TC-1.5 (UART console operational at boot) | Covered |
| FR-5.2 | Must | TC-4.1 | Covered |
| FR-5.3 | Must | TC-4.1 | Covered |
| FR-5.4 | Must | TC-4.2 | Covered |
| FR-5.5 | Must | TC-4.3, TC-4.4 | Covered |
| FR-5.6 | Must | TC-4.5 | Covered |
| FR-5.7 | Must | TC-3.1, TC-3.2 (`status` output verified) | Covered |
| FR-5.8 | Should | TC-4.1 (`help` exercised) | Covered |
| FR-5.9 | Should | TC-4.1 (unknown command exercised) | Covered |
| FR-6.1 | Should | TC-3.1 (`status` includes EFLG) | Covered |
| FR-6.2 | Should | — | GAP — requires deliberate hardware RX overflow; manual test only |
| FR-6.3 | May | — | GAP — sniffer does not transmit; bus-off not expected |
| NFR-1.1 | Must | TC-3.2, AT-2 | Covered |
| NFR-1.2 | Must | TC-3.2 (zero drops implies ISR within timing budget) | Covered |
| NFR-2.1 | Must | Makefile compiler flags review | Covered |
| NFR-2.2 | Must | — | GAP — SPI clock rate not directly tested; verify in SPI init code |
| NFR-3.1 | Should | TC-3.2 (no backpressure under burst) | Covered |
| NFR-3.2 | Should | TC-4.1 (manual timing) | Covered |
| NFR-4.1 | Should | AT-3 | Covered |

---

## 9. Troubleshooting Guide

| Symptom | Likely Cause | Diagnostic Steps | Corrective Action |
|---|---|---|---|
| Boot banner not printed | USART3 not initialised; ST-Link VCP not enumerated | Check `/dev/ttyACM*` after USB connect; try different cable | Verify USART3 clock enable (RCC_APB1ENR); check PD8/PD9 AF7 config |
| `MCP2515 init failed` | SPI wiring error; VCC at 5V; wrong SPI mode | Scope SCK/MOSI/CS during init; measure VCC | Re-check pin connections; confirm VCC = 3.3V; verify CPOL=0 CPHA=0 |
| MCP2515 init OK but no frames after `start` | EXTI0 not triggering; filter blocking all; bus not connected | Toggle debug GPIO in EXTI0_IRQHandler; check NVIC enable | Enable EXTI0 in NVIC; run `set filter 00000000 00000000` |
| Frames appear with wrong ID / garbled | Crystal mismatch (8 MHz expected, other fitted) | Oscilloscope on MCP2515 OSC1 pin; measure frequency | Recalculate CNF registers for actual crystal (see Appendix A.2) |
| Decoded values are wrong | Byte order error; wrong offset in decode formula | Compare raw bytes from sniffer against generator inject exact payload | Cross-check decode formulas in Section 6.3 and Appendix A.4 |
| `Frames dropped` > 0 | UART printing blocking ring buffer drain; ISR too slow | Check main loop drain rate; measure ISR wall time with debug GPIO | Reduce per-frame output during burst; increase ring buffer size |
| EFLG shows RX0OVR / RX1OVR | MCP2515 hardware RX buffers overflowed before ISR serviced them | Check NVIC priority of EXTI0 vs other ISRs | Raise EXTI0 priority; verify no blocking code in higher-priority ISRs |

---

## 10. Appendix

### A.1 STM32 HAL Peripheral Init Sketches

#### SPI1 (Master, CPOL=0/CPHA=0, ~703 kHz)

```c
SPI_HandleTypeDef hspi1 = {
    .Instance               = SPI1,
    .Init.Mode              = SPI_MODE_MASTER,
    .Init.Direction         = SPI_DIRECTION_2LINES,
    .Init.DataSize          = SPI_DATASIZE_8BIT,
    .Init.CLKPolarity       = SPI_POLARITY_LOW,
    .Init.CLKPhase          = SPI_PHASE_1EDGE,
    .Init.NSS               = SPI_NSS_SOFT,
    .Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128, /* 90 MHz / 128 ≈ 703 kHz */
    .Init.FirstBit          = SPI_FIRSTBIT_MSB,
    .Init.TIMode            = SPI_TIMODE_DISABLE,
    .Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE,
};
HAL_SPI_Init(&hspi1);
```

#### EXTI0 (PC0, falling edge, pull-up, priority 1)

NVIC priority is set at init time; EXTI0 is only **enabled** (`HAL_NVIC_EnableIRQ`) when the `start` command is received, and **disabled** on `stop`.

```c
/* At init time — configure GPIO and set priority, but do NOT enable IRQ yet */
GPIO_InitTypeDef g = {
    .Pin  = GPIO_PIN_0,
    .Mode = GPIO_MODE_IT_FALLING,
    .Pull = GPIO_PULLUP,
};
HAL_GPIO_Init(GPIOC, &g);
HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
/* HAL_NVIC_EnableIRQ called later from 'start' command handler */

/* On 'start' command */
HAL_NVIC_EnableIRQ(EXTI0_IRQn);

/* On 'stop' command */
HAL_NVIC_DisableIRQ(EXTI0_IRQn);
```

#### EXTI0 IRQ Handler skeleton

The handler clears the pending bit before the SPI read so a second arriving frame does not re-assert the line before the ISR returns.

```c
void EXTI0_IRQHandler(void)
{
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
```

---

### A.2 MCP2515 Baud Rate Register Values

**250 kbit/s — 8 MHz Crystal** (default)

Fosc = 8 MHz, TQ = 2 × (1/8 MHz) = 250 ns, bit time = 16 TQ = 4 µs → 250 kbit/s.

| Register | Value | Field breakdown |
|---|---|---|
| CNF1 | 0x00 | BRP = 0 (prescaler = 2); SJW = 1 TQ |
| CNF2 | 0xB1 | BTLMODE = 1; SAM = 0; PHSEG1 = 6 TQ; PRSEG = 2 TQ |
| CNF3 | 0x05 | PHSEG2 = 6 TQ |

Bit timing: Sync (1) + PropSeg (2) + PhSeg1 (6) + PhSeg2 (6) = 16 TQ → 250 kbit/s ✓  
Sample point: 81.25% (after PhSeg1).

**125 kbit/s — 8 MHz Crystal** (`set baud 125`)

| Register | Value | Notes |
|---|---|---|
| CNF1 | 0x01 | BRP = 1 (prescaler = 4); SJW = 1 TQ |
| CNF2 | 0xB1 | Unchanged |
| CNF3 | 0x05 | Unchanged |

**500 kbit/s — 8 MHz Crystal** (`set baud 500`)

| Register | Value | Notes |
|---|---|---|
| CNF1 | 0x00 | BRP = 0 |
| CNF2 | 0x90 | BTLMODE=1; PHSEG1=3 TQ; PRSEG=1 TQ (8 TQ total) |
| CNF3 | 0x02 | PHSEG2 = 3 TQ |

> If the MCP2515 crystal is 16 MHz, use BRP = 1 for 250 kbit/s and BRP = 3 for 125 kbit/s (CNF1 += 1 relative to each 8 MHz row above).

---

### A.3 MCP2515 Extended Frame RX Register Reconstruction

To reconstruct the 29-bit CAN ID from the MCP2515 RXB0/RXB1 registers:

```c
uint8_t sidh = mcp2515_read_reg(dev, MCP_RXB0SIDH);
uint8_t sidl = mcp2515_read_reg(dev, MCP_RXB0SIDL);
uint8_t eid8 = mcp2515_read_reg(dev, MCP_RXB0EID8);
uint8_t eid0 = mcp2515_read_reg(dev, MCP_RXB0EID0);
uint8_t dlc  = mcp2515_read_reg(dev, MCP_RXB0DLC) & 0x0F;

bool extended = (sidl & (1 << 3)) != 0;  /* IDE bit in RXBnSIDL */

uint32_t id;
if (extended) {
    id = ((uint32_t)sidh << 21)
       | (((uint32_t)(sidl & 0xE0)) << 13)
       | (((uint32_t)(sidl & 0x03)) << 16)
       | ((uint32_t)eid8 << 8)
       |  (uint32_t)eid0;
    id |= (1UL << 31);  /* mark as extended in can_rx_frame_t */
} else {
    id = ((uint32_t)sidh << 3) | ((sidl >> 5) & 0x07);
}
```

Use the MCP2515 `READ RX BUFFER` SPI instruction (0x90 for RXB0 from SIDH, 0x92 for RXB0 from D0) to read all registers in a single SPI transaction to minimise ISR duration.

---

### A.4 J1939 PGN Decode Reference

PGN extraction from 29-bit ID (for PDU2 format, PF ≥ 0xF0, PS is group extension):

```c
uint8_t  sa  = (uint8_t)(id & 0xFF);
uint8_t  pf  = (uint8_t)((id >> 16) & 0xFF);
uint32_t pgn = (id >> 8) & 0x3FFFF;
if (pf < 0xF0) pgn &= 0x3FF00;  /* PDU1: clear PS (destination addr) */
```

| PGN | Name | CAN ID (SA=0x00) | Decode |
|---|---|---|---|
| 61444 (0xF004) | EEC1 — Engine Speed | `0x0CF00400` | `rpm = (data[4]<<8 | data[3]) * 0.125f` |
| 65262 (0xFEEE) | Engine Temperature 1 | `0x18FEEE00` | `coolant_c = (int)data[0] - 40` |
| 65265 (0xFEF1) | CCVS — Vehicle Speed | `0x18FEF100` | `speed_kmh = (data[1]<<8 | data[0]) / 256.0f` |
| 65276 (0xFEFC) | Dash Display — Fuel | `0x18FEFC00` | `fuel_pct = data[1] * 0.4f` |

---

### A.5 Example UART Session

```
CAN Bus Sniffer — ready
STM32 Nucleo F446ZE | MCP2515 CANSTAT=0x00 | 250 kbit/s
Type 'help' for commands.
> start
OK: capture started
[  1234.100] 29b 0x0CF00400 [8] 00 7D 7D 14 00 FF 0F 7D  PGN61444 EEC1 RPM=850.0
[  1234.200] 29b 0x0CF00400 [8] 00 7D 7D 1C 00 FF 0F 7D  PGN61444 EEC1 RPM=851.9
[  2234.512] 29b 0x18FEEE00 [8] 73 FF FF FF FF FF FF FF  PGN65262 EngTemp Coolant=75C
[  2234.513] 29b 0x18FEF100 [8] 00 00 FF FF FF FF FF FF  PGN65265 CCVS Speed=0.0km/h
[  2234.514] 29b 0x18FEFC00 [8] FF 4B FF FF FF FF FF FF  PGN65276 Dash Fuel=30.0%
> status
State:          CAPTURING
Baud rate:      250 kbit/s
Frames RX:      112
Frames dropped: 0
MCP2515 EFLG:   0x00 (no errors)
> set filter 0CF00400 1FFFFFFF
OK: filter set
> stop
OK: capture stopped
> clear
OK: counters cleared
> help
Commands:
  start                        -- begin CAN capture
  stop                         -- halt capture
  set baud <125|250|500>       -- set bus baud rate (kbit/s)
  set filter <id_hex> <mask>   -- set acceptance filter (29-bit IDs)
  clear                        -- reset frame counters
  status                       -- show state and statistics
  help                         -- this message
>
```
