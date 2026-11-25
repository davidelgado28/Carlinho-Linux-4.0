#include "console.h"
#include "mouse.h"
#include "ports.h"

static u8 packet[3];
static u8 packet_index;

static u8 wait_input_clear(void)
{
    for (u32 attempt = 0; attempt < 100000; attempt++) {
        if ((inb(0x64) & 0x02) == 0) {
            return 1;
        }
    }
    return 0;
}

static u8 wait_output_full(void)
{
    for (u32 attempt = 0; attempt < 100000; attempt++) {
        if ((inb(0x64) & 0x01) != 0) {
            return 1;
        }
    }
    return 0;
}

static u8 mouse_command(u8 command)
{
    if (!wait_input_clear()) {
        return 0;
    }
    outb(0x64, 0xD4);
    if (!wait_input_clear()) {
        return 0;
    }
    outb(0x60, command);
    if (!wait_output_full()) {
        return 0;
    }
    return inb(0x60) == 0xFA;
}

u8 mouse_init(void)
{
    if (!wait_input_clear()) {
        return 0;
    }
    outb(0x64, 0xA8);

    if (!wait_input_clear()) {
        return 0;
    }
    outb(0x64, 0x20);
    if (!wait_output_full()) {
        return 0;
    }

    u8 configuration = inb(0x60);
    configuration |= 0x02;
    configuration &= (u8)~0x20;
    if (!wait_input_clear()) {
        return 0;
    }
    outb(0x64, 0x60);
    if (!wait_input_clear()) {
        return 0;
    }
    outb(0x60, configuration);

    if (!mouse_command(0xF6) || !mouse_command(0xF4)) {
        return 0;
    }

    packet_index = 0;
    return 1;
}

void mouse_interrupt(void)
{
    u8 value = inb(0x60);

    if (packet_index == 0 && (value & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = value;
    if (packet_index == 3) {
        i32 delta_x = (i8)packet[1];
        i32 delta_y = (i8)packet[2];
        console_mouse_event(delta_x, delta_y, packet[0] & 0x07);
        packet_index = 0;
    }
}
