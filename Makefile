TARGET  = can_sniffer
BUILD   = build

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc -x assembler-with-cpp
CP      = arm-none-eabi-objcopy
SZ      = arm-none-eabi-size

HAL_DIR  = Drivers/STM32F4xx_HAL_Driver
CMSIS_F4 = Drivers/CMSIS_Device_ST_STM32F4xx
CMSIS_5  = Drivers/CMSIS_5

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

CFLAGS  = $(MCU) -DSTM32F446xx -DUSE_HAL_DRIVER
CFLAGS += -ICore/Inc
CFLAGS += -I$(HAL_DIR)/Inc
CFLAGS += -I$(CMSIS_F4)/Include
CFLAGS += -I$(CMSIS_5)/CMSIS/Core/Include
CFLAGS += -Os -Wall -Wextra -ffunction-sections -fdata-sections -std=c11

ASFLAGS = $(MCU)

LDFLAGS = $(MCU) -TSTM32F446ZETx_FLASH.ld
LDFLAGS += -specs=nano.specs -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map
LDFLAGS += -lc -lm -lnosys

# Application sources
C_SRCS  = Core/Src/main.c
C_SRCS += Core/Src/mcp2515.c
C_SRCS += Core/Src/ring_buffer.c
C_SRCS += Core/Src/j1939_decode.c
C_SRCS += Core/Src/uart_console.c
C_SRCS += Core/Src/system_stm32f4xx.c
C_SRCS += Core/Src/stm32f4xx_it.c
C_SRCS += Core/Src/syscalls.c

# HAL sources — only what we use
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_cortex.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_rcc.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_rcc_ex.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_gpio.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_dma.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_dma_ex.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_spi.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_uart.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_pwr.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_pwr_ex.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_flash.c
C_SRCS += $(HAL_DIR)/Src/stm32f4xx_hal_flash_ex.c

AS_SRCS = Startup/startup_stm32f446zetx.s

OBJS = $(addprefix $(BUILD)/,$(C_SRCS:.c=.o))
OBJS += $(addprefix $(BUILD)/,$(AS_SRCS:.s=.o))

.PHONY: all clean flash

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex
	$(SZ) $<

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(CP) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(CP) -O ihex $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -c $(ASFLAGS) $< -o $@

flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	  -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD)
