#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pit.h"
#include "ports.h"
#include "serial.h"
#include "shell.h"

void kernel_main(u32 multiboot_info)
{
    (void)multiboot_info;
    serial_init();
    console_init();
    gdt_init();
    idt_init();
    pit_init(100);
    keyboard_init();
    console_set_color(15, 1);
    console_write(" Carlinho-OS ");
    console_set_color(15, 0);
    console_write("32-bit kernel\n");
    console_write("Carlinho-OS boot OK\n\n");
    shell_init();
    interrupts_enable();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
