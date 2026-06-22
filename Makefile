# Cajun Panda's GBA Audio Test ROM. Builds with stock arm-none-eabi GCC (no devkitARM).
PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

TARGET  := gba-audio-test
ARCH    := -mcpu=arm7tdmi -marm
CFLAGS  := $(ARCH) -O2 -ffreestanding -fno-builtin -nostdlib \
           -Wall -Wextra -ffunction-sections -fdata-sections
LDFLAGS := $(ARCH) -nostdlib -Wl,-T,gba.ld -Wl,--gc-sections \
           -Wl,-Map,$(TARGET).map

OBJS := src/crt0.o src/main.o src/screen.o
HDRS := src/gba.h src/sintab.h src/screen.h src/font.h

all: $(TARGET).gba

src/%.o: src/%.s
	$(CC) $(ARCH) -c $< -o $@

src/%.o: src/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) gba.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	python3 tools/gbafix.py $@
	@echo "Built $@ ($$(stat -c%s $@) bytes)"

run: $(TARGET).gba
	mgba-qt $(TARGET).gba

clean:
	rm -f src/*.o $(TARGET).elf $(TARGET).gba $(TARGET).map

.PHONY: all run clean
