# ------------------------------------------------
# Tên dự án và thư mục xuất build
# ------------------------------------------------
TARGET = firmware
BUILD_DIR = build

# ------------------------------------------------
# Danh sách file nguồn C và ASM
# ------------------------------------------------
C_SOURCES = \
Core/Src/main.c \
Driver/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c \
Driver/STM32F1xx_LL_Driver/Src/stm32f1xx_ll_gpio.c \
Driver/STM32F1xx_LL_Driver/Src/stm32f1xx_ll_rcc.c \
Driver/STM32F1xx_LL_Driver/Src/stm32f1xx_ll_utils.c

ASM_SOURCES = \
Driver/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s

# Linker Script
LDSCRIPT = STM32F103xx_FLASH.ld

# ------------------------------------------------
# Toolchain
# ------------------------------------------------
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

# ------------------------------------------------
# Cờ biên dịch (Flags)
# ------------------------------------------------
CPU = -mcpu=cortex-m3
MCU = $(CPU) -mthumb

# Macros C
C_DEFS = \
-DSTM32F103xB \
-DUSE_FULL_LL_DRIVER

# Đường dẫn Includes (-I)
C_INCLUDES = \
-ICore/Inc \
-IDriver/CMSIS/Include \
-IDriver/CMSIS/Device/ST/STM32F1xx/Include \
-IDriver/STM32F1xx_LL_Driver/Inc

# CFLAGS
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) -O0 -Wall -fdata-sections -ffunction-sections -std=gnu11

# LDFLAGS (Cờ cho Linker)
LDFLAGS = $(MCU) -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# ------------------------------------------------
# Gom danh sách các object file vào build/
# ------------------------------------------------
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# ------------------------------------------------
# Các lệnh build (Targets)
# ------------------------------------------------
.PHONY: all clean flash

all: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@echo "Assembling $<..."
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	@echo "Linking $@..."
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(CP) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(CP) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $(BUILD_DIR)/$(TARGET).bin 0x08000000
	st-flash reset