# STM32 CAN Bus Sniffer

CAN bus sniffer and live J1939 decoder for STM32 Nucleo F446ZE.

**Hardware:** STM32 Nucleo F446ZE + MCP2515 SPI CAN module  
**Bus:** 250 kbit/s, SAE J1939  
**Firmware:** Bare-metal STM32 HAL, no RTOS  

## Features

- Interrupt-driven frame capture via MCP2515 INT pin (PC0 EXTI0); polling fallback in main loop ensures zero drops even if INT wire is not connected
- Ring buffer (256 entries, power-of-two) — zero frame drops under burst load
- CAN 2.0A (11-bit) and CAN 2.0B (29-bit extended) support
- SAE J1939 PGN decoding: 61444 (RPM), 65262 (Coolant), 65265 (Speed), 65276 (Fuel)
- Live timestamped decoded output on UART terminal (USART3 / ST-Link VCP)
- SPI at 703 kHz (APB2 ÷ 128) — conservative rate for bench wiring noise tolerance

## UART Commands

| Command | Description |
|---|---|
| `start` | Begin capture |
| `stop` | Halt capture |
| `set baud <125\|250\|500>` | Set CAN bus baud rate (kbit/s) |
| `set filter <id> <mask>` | Set MCP2515 acceptance filter (29-bit hex IDs) |
| `clear` | Reset frame counters |
| `status` | Show state, frame count, drop count, EFLG |
| `help` | Print command list |

## Pin Connections (SPI1)

| MCP2515 | STM32 Pin | Connector | Notes |
|---|---|---|---|
| VCC | 3.3V | — | Must be 3.3V — 5V damages GPIO |
| GND | GND | — | |
| SCK | PA5 | Arduino CN7 D13 | SPI1_SCK AF5 |
| SI (MOSI) | PA7 | Arduino CN7 D11 | SPI1_MOSI AF5 |
| SO (MISO) | PA6 | Arduino CN7 D12 | SPI1_MISO AF5 |
| CS | PA4 | ZIO CN11 | GPIO output, software-driven; active-low |
| INT | PC0 | Arduino CN9 A5 | EXTI0, falling-edge, pull-up |

> **Note:** CS was moved from PB6 to PA4 (ZIO CN11) — PA4 is SPI1_NSS AF5 on the F446ZE and has proven continuity on the Nucleo ZIO header. Do not use PB6 for this function.

## Companion Project

[esp32-can-traffic-generator](https://github.com/ozturk35/esp32-can-traffic-generator) — ESP32-S3 J1939 traffic generator (generator node on the same bus).

## Documentation

See [`Documents/stm32-can-bus-sniffer-fsd.md`](Documents/stm32-can-bus-sniffer-fsd.md) for the full Functional Specification Document.

## Build

```bash
make -j8
```

## Flash

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/can_sniffer.hex verify reset exit"
```
