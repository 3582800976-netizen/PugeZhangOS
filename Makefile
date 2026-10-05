# Each stage has its own build directory, so LAB/CPUS changes never reuse objects.
TOOLPREFIX ?= riscv64-linux-gnu-
CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
QEMU ?= qemu-system-riscv64
LAB ?= 3
CPUS ?= 3
BIOS ?= default

ifeq ($(filter $(LAB),1 2 3),)
$(error LAB must be 1, 2 or 3)
endif
ifeq ($(filter $(CPUS),1 2 3 4 5 6 7 8),)
$(error CPUS must be between 1 and 8)
endif

BUILD = build/lab$(LAB)-cpu$(CPUS)
TARGET = $(BUILD)/kernel.elf
CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany \
         -ffreestanding -fno-builtin -nostdlib -nostartfiles \
         -fno-pic -fno-pie -fno-stack-protector -mno-relax \
         -Wall -Wextra -Werror -O2 -g -MMD -MP -Iinclude \
         -DLAB=$(LAB) -DCPUS=$(CPUS)
LDFLAGS = -T linker.ld --defsym=KERNEL_BASE=$(KBASE)
ASM_SRCS = boot/entry.S boot/trap.S
C_SRCS = boot/start.c kernel/main.c kernel/spinlock.c kernel/trap.c \
         drivers/uart.c drivers/plic.c lib/console.c lib/memory.c lib/sbi.c
ifeq ($(LAB),1)
KBASE = 0x80000000
BIOS = none
else
KBASE = 0x80200000
C_SRCS += kernel/page.c kernel/vm.c
endif
OBJS = $(addprefix $(BUILD)/,$(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o))

.PHONY: all qemu qemu-gdb check clean
all: $(TARGET)

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

qemu: $(TARGET)
	$(QEMU) -machine virt -bios $(BIOS) -kernel $(TARGET) -m 128M -smp $(CPUS) -nographic

# A separate terminal can use: gdb-multiarch <kernel.elf>; target remote :1234
qemu-gdb: $(TARGET)
	$(QEMU) -machine virt -bios $(BIOS) -kernel $(TARGET) -m 128M -smp $(CPUS) -nographic -S -gdb tcp:127.0.0.1:1234

check:
	python3 scripts/check.py

clean:
	python3 -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'

-include $(OBJS:.o=.d)
