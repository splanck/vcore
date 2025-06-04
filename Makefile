CROSS ?=
NASM = nasm
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
CFLAGS = -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c
LDFLAGS = -nostdlib

.PHONY: all kernel bootloader users clean

all: kernel bootloader users

# Kernel build
kernel:
	nasm -f elf64 -o kernel.o src/kernel/kernel.asm
	nasm -f elf64 -o trapa.o src/kernel/trap.asm
	nasm -f elf64 -o liba.o src/kernel/lib.asm
	$(CC) $(CFLAGS) src/kernel/main.c
	$(CC) $(CFLAGS) src/kernel/trap.c
	$(CC) $(CFLAGS) src/kernel/print.c
	$(CC) $(CFLAGS) src/kernel/debug.c
	$(CC) $(CFLAGS) src/kernel/memory.c
	$(CC) $(CFLAGS) src/kernel/process.c
	$(CC) $(CFLAGS) src/kernel/syscall.c
	$(CC) $(CFLAGS) src/kernel/lib.c
	$(CC) $(CFLAGS) src/kernel/keyboard.c
	$(CC) $(CFLAGS) src/kernel/file.c
	$(LD) $(LDFLAGS) -T link.lds -o kernel kernel.o main.o trapa.o trap.o liba.o print.o debug.o memory.o process.o syscall.o lib.o keyboard.o file.o
	$(OBJCOPY) -O binary kernel kernel.bin
	
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
	
os.img: boot/boot.bin boot/loader/loader.bin
	rm -f $@
	dd if=boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=boot/loader/loader.bin of=$@ bs=512 count=15 seek=1 conv=notrunc

# User programs
users: user/ls/ls user/test/test.bin user/totalmem/totalmem user/user1/user.bin

user/ls/ls:
				cd user/ls && \
				$(NASM) -f elf64 -o start.o start.asm && \
				$(CC) $(CFLAGS) -c main.c && \
				$(LD) $(LDFLAGS) -T link.lds -o user start.o main.o lib.a && \
		$(OBJCOPY) -O binary user ls
	
user/test/test.bin:
		cd user/test && \
		$(NASM) -f elf64 -o start.o start.asm && \
		$(CC) $(CFLAGS) -c main.c && \
		$(LD) $(LDFLAGS) -T link.lds -o user start.o main.o lib.a && \
$(OBJCOPY) -O binary user test.bin

user/totalmem/totalmem:
			cd user/totalmem && \
			$(NASM) -f elf64 -o start.o start.asm && \
			$(CC) $(CFLAGS) -c main.c && \
			$(LD) $(LDFLAGS) -T link.lds -o user start.o main.o lib.a && \
	$(OBJCOPY) -O binary user totalmem
	
user/user1/user.bin:
		cd user/user1 && \
		$(NASM) -f elf64 -o start.o start.asm && \
		$(CC) $(CFLAGS) -c main.c && \
		$(LD) $(LDFLAGS) -T link.lds -o user start.o main.o lib.a && \
$(OBJCOPY) -O binary user user.bin

clean:
	rm -f *.o kernel kernel.bin
	rm -f boot/boot.bin boot/loader/*.o boot/loader/entry boot/loader/entry.bin boot/loader/loader.bin os.img
	rm -f user/*/*.o user/*/user user/*/*.bin

