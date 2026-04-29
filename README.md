# STM32 CAN Bus Sniffer

CAN bus sniffer and live J1939 decoder for STM32 Nucleo F446ZE.

**Hardware:** STM32 Nucleo F446ZE + MCP2515 SPI CAN module  
**Bus:** 250 kbit/s, SAE J1939  
**Firmware:** Bare-metal STM32 HAL, no RTOS  

## Features

- Interrupt-driven frame capture via MCP2515 INT pin (PC0 EXTI)
- Ring buffer (256 entries) — zero frame drops under burst load
- CAN 2.0A (11-bit) and CAN 2.0B (29-bit extended) support
- SAE J1939 PGN decoding: 61444 (RPM), 65262 (Coolant), 65265 (Speed), 65276 (Fuel)
- Live timestamped decoded output on UART terminal (USART3 / ST-Link VCP)

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

| MCP2515 | STM32 | Notes |
|---|---|---|
| VCC | 3.3V | Must be 3.3V |
| SCK | PA5 | SPI1_SCK |
| MOSI | PA7 | SPI1_MOSI |
| MISO | PA6 | SPI1_MISO |
| CS | PB6 | GPIO output |
| INT | PC0 | EXTI0 interrupt |

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
  -c "program build/can_sniffer.elf verify reset exit"
```
