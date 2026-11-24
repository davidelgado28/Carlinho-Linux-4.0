# Carlinho-OS

Carlinho-OS is a small 32-bit x86 operating system built from a freestanding C kernel and NASM assembly. GRUB loads the kernel using the Multiboot2 protocol. The project is a native OS foundation, separate from Carlinho WebOS: browser applications and its browser-backed virtual filesystem do not run inside this kernel.

The kernel initializes its own GDT and IDT, remaps the PIC, configures the PIT, handles PS/2 keyboard input, writes to VGA text memory and the serial port, and provides an interactive command shell. The shell currently supports `help`, `about`, `clear`, `echo`, `uptime`, `reboot`, and `halt`. Storage, a filesystem, user processes, and graphical applications are not implemented yet.

## Build requirements

Install NASM, GRUB utilities, xorriso, and an i686-elf cross-compiler. GNU Make uses `i686-elf-gcc` by default. On Debian or Ubuntu, the required GRUB and ISO packages are `grub-pc-bin`, `grub-common`, `xorriso`, and `mtools`.

To build the kernel and ISO:

```sh
make
```

The resulting image is `build/carlinho.iso`. To build with another GCC-compatible 32-bit compiler, set `CROSS` to its tool prefix. For example, `make CROSS=` uses the system `gcc` with `-m32`.

## Run in QEMU

```sh
qemu-system-i386 -cdrom build/carlinho.iso
```

The GitHub Actions workflow builds the ISO on every push and pull request, boots it in headless QEMU, and uploads the ISO and SHA-256 checksum as an artifact. Pushing a `v*` tag also publishes both files as a GitHub Release.
