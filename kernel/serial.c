#include "ports.h"
#include "serial.h"

#define SERIAL_PORT 0x3F8

void serial_init(void)
{
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x80);
    outb(SERIAL_PORT + 0, 0x03);
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);
    outb(SERIAL_PORT + 2, 0xC7);
    outb(SERIAL_PORT + 4, 0x0B);
}

void serial_write_char(char character)
{
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0) {
    }

    if (character == '\n') {
        outb(SERIAL_PORT, '\r');
        while ((inb(SERIAL_PORT + 5) & 0x20) == 0) {
        }
    }

    outb(SERIAL_PORT, (u8)character);
}
