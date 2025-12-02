# Makefile with threading support

CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS = -I. -march=rv64gc -mabi=lp64 -mcmodel=medany -O2 -ffreestanding -nostdlib -fno-builtin -Wall
LDFLAGS = -T linker.ld

# Source files (include threading)
SRCS = entry.S kernel.c uart.c string.c apps.c thread.c thread_trampoline.c context.S fs.c sync.c prog.c
OBJS = entry.o kernel.o uart.o string.o apps.o thread.o thread_trampoline.o context.o fs.o sync.o prog.o

all: kernel.bin

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.elf $(OBJS) --entry=_start

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

clean:
	rm -f *.o kernel.elf kernel.bin

.PHONY: all clean
