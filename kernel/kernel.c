#include "console.h"
#include "fs.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "mouse.h"
#include "pit.h"
#include "ports.h"
#include "serial.h"
#include "shell.h"

void kernel_main(u32 multiboot_info)
{
    serial_init();
    console_init(multiboot_info);
    gdt_init();
    idt_init();
    pit_init(100);
    keyboard_init();
    u8 has_mouse = mouse_init();
    fs_init();
    console_set_color(15, 1);
    console_write(" Carlinho-OS ");
    console_set_color(15, 0);
    console_write("32-bit kernel\n");
    console_write("Carlinho-OS boot OK\n\n");
    if (console_is_graphical()) {
        console_write("GRAPHICAL FRAMEBUFFER ACTIVE\n");
    } else {
        console_write("Framebuffer unavailable; using VGA text mode.\n");
    }
    if (has_mouse) {
        console_write("PS/2 mouse ready\n");
    }
    shell_init();
    interrupts_enable();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
