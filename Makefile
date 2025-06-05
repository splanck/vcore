CROSS ?=
NASM = nasm
CC = $(CROSS)gcc
	LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
AR = $(CROSS)ar
	OBJDIR ?= build
CFLAGS = -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c -Isrc
LDFLAGS = -nostdlib
	
C_SRCS := $(wildcard src/kernel/*.c)
DRIVER_SRCS := $(wildcard src/drivers/net/*.c)
NET_SRCS := $(wildcard src/net/*.c)
ASM_SRCS := $(wildcard src/arch/x86/*.asm)
C_OBJS := $(patsubst src/kernel/%.c,$(OBJDIR)/%.o,$(C_SRCS)) \
$(patsubst src/drivers/net/%.c,$(OBJDIR)/%.o,$(DRIVER_SRCS)) \
$(patsubst src/net/%.c,$(OBJDIR)/%.o,$(NET_SRCS))
ASM_OBJS := $(patsubst src/arch/x86/%.asm,$(OBJDIR)/%_asm.o,$(ASM_SRCS))
KERNEL_OBJS := $(ASM_OBJS) $(C_OBJS)
FS_IMG := fs.img

LIBC_C_SRCS := libc/src/printf.c libc/src/stdlib.c libc/src/string.c \
               libc/src/stdio.c libc/src/ctype.c libc/src/strtol.c \
               libc/src/errno.c
LIBC_ASM_SRCS := $(wildcard libc/src/*.asm)
LIBC_C_OBJS := $(patsubst libc/src/%.c,$(OBJDIR)/libc_%.o,$(LIBC_C_SRCS))
LIBC_ASM_OBJS := $(patsubst libc/src/%.asm,$(OBJDIR)/libc_%.o,$(LIBC_ASM_SRCS))
LIBC_OBJS := $(LIBC_C_OBJS) $(LIBC_ASM_OBJS)
LIBC_ARCHIVE := libc/libc.a

$(OBJDIR):
	mkdir -p $@

.PHONY: all libc kernel bootloader users clean run

all: libc kernel bootloader users

libc: $(LIBC_ARCHIVE)

# Kernel build
kernel: $(OBJDIR) $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -T link.lds -o kernel.elf $(KERNEL_OBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin

$(OBJDIR)/%.o: src/kernel/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: src/drivers/net/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: src/net/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(OBJDIR)/%_asm.o: src/arch/x86/%.asm | $(OBJDIR)
	$(NASM) -f elf64 -o $@ $<

$(OBJDIR)/libc_%.o: libc/src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I libc/include -o $@ $<
$(OBJDIR)/libc_%.o: libc/src/%.asm | $(OBJDIR)
	$(NASM) -f elf64 -o $@ $<

$(LIBC_ARCHIVE): $(LIBC_OBJS)
	$(AR) rcs $@ $^
# Bootloader build
bootloader: os.img

boot/boot.bin: boot/boot.asm
	$(NASM) -f bin -o $@ $<
	
boot/loader/entry.bin:
	cd boot/loader && \
	$(NASM) -f elf64 -o entry.o entry.asm && \
	$(NASM) -f elf64 -o liba.o lib.asm && \
	$(CC) $(CFLAGS) -c main.c && \
	$(CC) $(CFLAGS) -c print.c && \
	$(CC) $(CFLAGS) -c debug.c && \
	$(CC) $(CFLAGS) -c file.c && \
	$(LD) $(LDFLAGS) -T link.lds -o entry entry.o main.o liba.o print.o debug.o file.o && \
	$(OBJCOPY) -O binary entry entry.bin

boot/loader/loader.bin: boot/loader/loader.asm boot/loader/entry.bin
			cd boot/loader && \
		$(NASM) -f bin -o loader.bin loader.asm && \
		dd if=entry.bin >> loader.bin
	
os.img: boot/boot.bin boot/loader/loader.bin $(FS_IMG)
	rm -f $@
	dd if=boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=boot/loader/loader.bin of=$@ bs=512 count=15 seek=1 conv=notrunc
	dd if=$(FS_IMG) of=$@ bs=512 seek=63 conv=notrunc

# User programs
users: libc user/ls/ls.elf user/test/test.elf user/totalmem/totalmem.elf user/user1/user.elf user/ping/ping.elf user/cow/cow.elf

$(FS_IMG): kernel.elf users boot/boot.bin
	python3 scripts/mkfs.py boot/boot.bin $(FS_IMG)

user/ls/ls.elf:
	cd user/ls && \
	$(NASM) -f elf64 -o start.o start.asm && \
	$(CC) $(CFLAGS) -I ../../libc/include -c main.c && \
       $(LD) $(LDFLAGS) -T link.lds -o ls.elf start.o main.o ../../libc/libc.a
	
user/test/test.elf:
	cd user/test && \
	$(NASM) -f elf64 -o start.o start.asm && \
	$(CC) $(CFLAGS) -I ../../libc/include -c main.c && \
       $(LD) $(LDFLAGS) -T link.lds -o test.elf start.o main.o ../../libc/libc.a

user/totalmem/totalmem.elf:
	cd user/totalmem && \
	$(NASM) -f elf64 -o start.o start.asm && \
	$(CC) $(CFLAGS) -I ../../libc/include -c main.c && \
       $(LD) $(LDFLAGS) -T link.lds -o totalmem.elf start.o main.o ../../libc/libc.a
user/user1/user.elf:
	cd user/user1 && \
	$(NASM) -f elf64 -o start.o start.asm && \
	$(CC) $(CFLAGS) -I ../../libc/include -c main.c && \
       $(LD) $(LDFLAGS) -T link.lds -o user.elf start.o main.o ../../libc/libc.a
user/ping/ping.elf:

	cd user/ping && \
	$(NASM) -f elf64 -o start.o start.asm && \
	$(CC) $(CFLAGS) -I ../../libc/include -c main.c && \
       $(LD) $(LDFLAGS) -T link.lds -o ping.elf start.o main.o ../../libc/libc.a

user/cow/cow.elf:
	cd user/cow && \
	$(NASM) -f elf64 -o start.o start.asm && \
	$(CC) $(CFLAGS) -I ../../libc/include -c main.c && \
       $(LD) $(LDFLAGS) -T link.lds -o cow.elf start.o main.o ../../libc/libc.a

clean:
	rm -rf $(OBJDIR) kernel.elf kernel.bin $(FS_IMG)
	rm -f boot/boot.bin boot/loader/*.o boot/loader/entry boot/loader/entry.bin boot/loader/loader.bin os.img
	rm -f user/*/*.o user/*/*.elf

run: os.img
	qemu-system-x86_64 -drive format=raw,file=os.img

