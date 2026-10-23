# Lab 5: independent kernel, separately linked U-mode heap demonstration.
TOOLPREFIX ?= riscv64-linux-gnu-
CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
QEMU ?= qemu-system-riscv64
CPUS ?= 3
CHECK_CPUS ?= 1,2,3,8

ifeq ($(filter $(CPUS),1 2 3 4 5 6 7 8),)
$(error CPUS must be between 1 and 8)
endif

.DEFAULT_GOAL := all
BUILD = build/cpu$(CPUS)
TARGET = $(BUILD)/kernel.elf
USER_BUILD = $(BUILD)/user
USER_ELF = $(USER_BUILD)/first.elf
USER_CODE = $(USER_BUILD)/code.bin
USER_DATA = $(USER_BUILD)/data.bin
# The hardware and boot mode are fixed for this lab.
BIOS = default
CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany \
         -ffreestanding -fno-builtin -nostdlib -nostartfiles \
         -fno-pic -fno-pie -fno-stack-protector -mno-relax \
         -Wall -Wextra -Werror -O2 -g -MMD -MP -Iinclude -DCPUS=$(CPUS)
LDFLAGS = -T linker.ld
USER_CFLAGS = $(CFLAGS) -Os -msmall-data-limit=0
USER_SRCS = user/start.S user/first.c user/register_probe.S user/usys.S
USER_OBJS = $(addprefix $(USER_BUILD)/,$(notdir $(USER_SRCS:.S=.o)))
USER_OBJS := $(USER_OBJS:.c=.o)

# No sources or headers are taken from another lab.
ASM_SRCS = $(wildcard boot/*.S trap/*.S) user/image.S
C_SRCS = $(wildcard boot/*.c kernel/*.c mm/*.c trap/*.c drivers/*.c \
                    proc/*.c syscall/*.c monitor/*.c tests/*.c lib/*.c)
OBJS = $(addprefix $(BUILD)/,$(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o))

.PHONY: all qemu qemu-gdb check clean
all: $(TARGET)

$(USER_BUILD)/%.o: user/%.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/%.o: user/%.S Makefile
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_ELF): $(USER_OBJS) user/linker.ld Makefile
	$(LD) -T user/linker.ld -o $@ $(USER_OBJS)

$(USER_CODE): $(USER_ELF)
	$(OBJCOPY) -O binary --only-section=.text $< $@

$(USER_DATA): $(USER_ELF)
	$(OBJCOPY) -O binary --only-section=.data $< $@

$(BUILD)/user/image.o: user/image.S $(USER_CODE) $(USER_DATA) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DUSER_CODE_IMAGE='"$(USER_CODE)"' \
	      -DUSER_DATA_IMAGE='"$(USER_DATA)"' -c $< -o $@

$(BUILD)/%.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) linker.ld Makefile
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

qemu: $(TARGET)
	$(QEMU) -machine virt -bios $(BIOS) -kernel $(TARGET) -m 128M -smp $(CPUS) -nographic

# Connect from another terminal: gdb-multiarch build/cpu3/kernel.elf
# Then: target remote :1234
qemu-gdb: $(TARGET)
	$(QEMU) -machine virt -bios $(BIOS) -kernel $(TARGET) -m 128M -smp $(CPUS) -nographic -S -gdb tcp:127.0.0.1:1234

check:
	python3 ../scripts/check.py --lab 5 --cpus "$(CHECK_CPUS)"

clean:
	python3 -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'

-include $(OBJS:.o=.d) $(USER_OBJS:.o=.d)
