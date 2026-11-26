# Carlinho-OS

Carlinho-OS is an experimental 32-bit x86 operating system built from a freestanding C kernel and NASM assembly. GRUB loads the kernel using the Multiboot2 protocol and requests an 800x600x32 framebuffer. The project is separate from Carlinho WebOS: its browser applications do not run inside this kernel.

The kernel initializes its own GDT and IDT, remaps the PIC, configures the PIT, handles a PS/2 keyboard and mouse, and draws a desktop inspired by Carlinho WebOS, including its wallpaper, desktop icons, glass-style dock, Start menu, and draggable, resizable, minimizable, and maximizable app windows. Notes can be edited and saved to the in-memory filesystem; the Terminal runs the shell. Browser and Music have native placeholder screens because networking and audio playback are not implemented.

The shell supports `ls` (`-a`, `-l`, and paths), `pwd`, `cd`, `cat`, `echo` (including `>` and `>>`), `touch`, `mkdir` (`-p`), `rm` (`-r`), `rmdir`, `cp`, `mv`, `wc`, `head`, `tail`, `grep`, `history`, `uname`, `whoami`, `hostname`, `uptime`, `clear`, `help`, `man`, `reboot`, and `halt`. Its small filesystem is volatile RAM storage. It is not Linux and cannot run every Linux command or Linux binary: processes, a disk filesystem, pipes, permissions, and POSIX compatibility are not implemented.

<img width="798" height="593" alt="image" src="https://github.com/user-attachments/assets/057efe9e-f4b5-4601-bbed-4fab248eb1f9" />


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

The GitHub Actions workflow builds the ISO on every push and pull request, boots it in headless QEMU, checks that the graphical framebuffer starts and that `ls` prints the initial file, and uploads the ISO and SHA-256 checksum as an artifact. Pushing a `v*` tag also publishes both files as a GitHub Release.
