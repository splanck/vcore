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
ASM_SRCS := $(sort $(wildcard src/arch/x86/*.asm) src/arch/x86/start32.asm)
C_OBJS := $(patsubst src/kernel/%.c,$(OBJDIR)/%.o,$(C_SRCS)) \
$(patsubst src/drivers/net/%.c,$(OBJDIR)/%.o,$(DRIVER_SRCS)) \
$(patsubst src/net/%.c,$(OBJDIR)/%.o,$(NET_SRCS))
ASM_OBJS := $(patsubst src/arch/x86/%.asm,$(OBJDIR)/%_asm.o,$(ASM_SRCS))
KERNEL_OBJS := $(ASM_OBJS) $(C_OBJS)
FS_IMG := fs.img
LOADER_BIN := boot/loader/loader.bin
ISO_DIR ?= isodir
ISO_IMG ?= os.iso

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

.PHONY: all libc kernel bootloader users clean run run-iso iso

HAVE_GRUB_MKRESCUE := $(shell command -v grub-mkrescue >/dev/null 2>&1 && echo yes)
ifeq ($(HAVE_GRUB_MKRESCUE),yes)
ALL_EXTRA := $(ISO_IMG)
endif

all: libc kernel bootloader users $(ALL_EXTRA)

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

boot/boot.bin: boot/boot.asm $(LOADER_BIN)
	LOADER_SECTORS=`python3 -c 'import os,math; \
	 print(math.ceil(os.path.getsize("$(LOADER_BIN)")/512))'`; \
$(NASM) -f bin -DLOADER_SECTORS=$$LOADER_SECTORS -o $@ $<
	
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
	ENTRY_SECTORS=`python3 -c 'import os,math; \
	print(math.ceil(os.path.getsize("entry.bin")/512))'` && \
	$(NASM) -f bin -DENTRY_SECTORS=$$ENTRY_SECTORS \
	-o loader.bin loader.asm && \
	dd if=entry.bin >> loader.bin
	
os.img: boot/boot.bin boot/loader/loader.bin $(FS_IMG)
	test -f $(FS_IMG)
	rm -f $@
	dd if=boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	LOADER_SECTORS=`python3 -c 'import os,math; \
	 print(math.ceil(os.path.getsize("$(LOADER_BIN)")/512))'`; \
	dd if=boot/loader/loader.bin of=$@ bs=512 \
	count=$$LOADER_SECTORS seek=1 conv=notrunc
	dd if=$(FS_IMG) of=$@ bs=512 seek=63 conv=notrunc
	@actual=$$(stat -c '%s' $@); \
	[ $$actual -ge 100000000 ] || \
	    { echo 'Error: os.img too small; filesystem missing?' >&2; exit 1; }

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
	rm -rf $(ISO_DIR) $(ISO_IMG)

run: os.img
	qemu-system-x86_64 -m 1024 -drive if=ide,format=raw,file=os.img -boot order=c

run-iso: $(ISO_IMG)
	qemu-system-x86_64 -cdrom $(ISO_IMG)

iso: $(ISO_IMG)


$(ISO_DIR)/boot/grub/grub.cfg: grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg


$(ISO_IMG): $(ISO_DIR)/boot/grub/grub.cfg kernel users
	command -v grub-mkrescue >/dev/null || \
	    { echo 'Error: grub-mkrescue not found.' >&2; exit 1; }
	cp kernel.elf $(ISO_DIR)
	cp user/*/*.elf $(ISO_DIR)
	grub-mkrescue -o $@ $(ISO_DIR) >/dev/null
	
