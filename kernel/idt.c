#include "console.h"
#include "idt.h"
#include "keyboard.h"
#include "mouse.h"
#include "pit.h"
#include "ports.h"
#include "types.h"

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 flags;
    u16 offset_high;
} PACKED;

struct idt_pointer {
    u16 limit;
    u32 base;
} PACKED;

static struct idt_entry entries[48];
static struct idt_pointer pointer;

extern u32 interrupt_stub_table[];
extern void idt_load(const struct idt_pointer *descriptor);

static void set_gate(u32 vector, u32 address)
{
    entries[vector].offset_low = (u16)(address & 0xFFFF);
    entries[vector].selector = 0x08;
    entries[vector].zero = 0;
    entries[vector].flags = 0x8E;
    entries[vector].offset_high = (u16)(address >> 16);
}

static void remap_pic(void)
{
    u8 master_mask = inb(0x21);
    u8 slave_mask = inb(0xA1);

    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();
    outb(0x21, (u8)((master_mask & 0xF8) | 0xF8));
    outb(0xA1, (u8)((slave_mask & 0xEF) | 0xEF));
}

void idt_init(void)
{
    pointer.limit = (u16)(sizeof(entries) - 1);
    pointer.base = (u32)entries;

    for (u32 vector = 0; vector < 48; vector++) {
        set_gate(vector, interrupt_stub_table[vector]);
    }

    remap_pic();
    idt_load(&pointer);
}

void interrupt_handler(struct interrupt_frame *frame)
{
    u32 vector = frame->interrupt_number;

    if (vector < 32) {
        interrupts_disable();
        console_write("\nKernel exception: ");
        console_write_u32(vector);
        console_write("\nSystem halted.\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    if (vector == 32) {
        pit_interrupt();
    } else if (vector == 33) {
        keyboard_interrupt();
    } else if (vector == 44) {
        mouse_interrupt();
    }

    if (vector >= 40) {
        outb(0xA0, 0x20);
    }

    if (vector >= 32 && vector < 48) {
        outb(0x20, 0x20);
    }
}
