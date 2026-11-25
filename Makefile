CROSS ?= i686-elf-
CC = $(CROSS)gcc
NASM = nasm
GRUB_MKRESCUE = grub-mkrescue
GRUB_FILE = grub-file

CFLAGS = -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -fno-unwind-tables -fno-asynchronous-unwind-tables -m32 -mno-sse -mno-mmx -msoft-float -O2 -Wall -Wextra -Werror -Ikernel/include
LDFLAGS = -m32 -nostdlib -no-pie -Wl,--build-id=none -Wl,-T,linker.ld
C_SOURCES = kernel/kernel.c kernel/console.c kernel/fs.c kernel/gdt.c kernel/idt.c kernel/keyboard.c kernel/mouse.c kernel/pit.c kernel/serial.c kernel/shell.c kernel/string.c
C_OBJECTS = $(patsubst kernel/%.c,build/%.o,$(C_SOURCES))
OBJECTS = build/boot.o build/interrupts.o build/wallpaper.o $(C_OBJECTS)

.PHONY: all iso clean check

all: iso

iso: build/carlinho.iso

build:
	mkdir -p build

build/boot.o: boot/boot.asm | build
	$(NASM) -f elf32 $< -o $@

build/interrupts.o: kernel/interrupts.asm | build
	$(NASM) -f elf32 $< -o $@

build/wallpaper.o: boot/wallpaper.asm assets/carlinho-webos.rgb565 | build
	$(NASM) -f elf32 $< -o $@

build/%.o: kernel/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)
	$(GRUB_FILE) --is-x86-multiboot2 $@

build/carlinho.iso: build/kernel.elf boot/grub/grub.cfg
	mkdir -p build/isofiles/boot/grub
	cp build/kernel.elf build/isofiles/boot/kernel.elf
	cp boot/grub/grub.cfg build/isofiles/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ build/isofiles

check: build/kernel.elf
	$(GRUB_FILE) --is-x86-multiboot2 $<

clean:
	rm -rf build
