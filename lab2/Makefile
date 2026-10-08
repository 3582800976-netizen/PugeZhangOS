# Lab 2: build only the sources in this directory.
TOOLPREFIX ?= riscv64-linux-gnu-
CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
QEMU ?= qemu-system-riscv64
CPUS ?= 3
CHECK_CPUS ?= 1,2,3,8

ifeq ($(filter $(CPUS),1 2 3 4 5 6 7 8),)
$(error CPUS must be between 1 and 8)
endif

.DEFAULT_GOAL := all
BUILD = build/cpu$(CPUS)
TARGET = $(BUILD)/kernel.elf
# The hardware and boot mode are fixed for this lab.
BIOS = default
CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany \
         -ffreestanding -fno-builtin -nostdlib -nostartfiles \
         -fno-pic -fno-pie -fno-stack-protector -mno-relax \
         -Wall -Wextra -Werror -O2 -g -MMD -MP -Iinclude -DCPUS=$(CPUS)
LDFLAGS = -T linker.ld

# No sources or headers are taken from another lab.
ASM_SRCS = $(wildcard boot/*.S trap/*.S)
C_SRCS = $(wildcard boot/*.c kernel/*.c mm/*.c trap/*.c drivers/*.c \
                    monitor/*.c tests/*.c lib/*.c)
OBJS = $(addprefix $(BUILD)/,$(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o))

.PHONY: all qemu qemu-gdb check clean
all: $(TARGET)

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
	python3 ../scripts/check.py --lab 2 --cpus "$(CHECK_CPUS)"

clean:
	python3 -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'

-include $(OBJS:.o=.d)
