#include "console.h"
#include "ports.h"
#include "serial.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile u16 *)0xB8000)

static u32 cursor_x;
static u32 cursor_y;
static u8 current_color;

static u8 make_color(u8 foreground, u8 background)
{
    return (u8)(foreground | (background << 4));
}

static u16 make_entry(char character, u8 color)
{
    return (u16)(u8)character | ((u16)color << 8);
}

static void update_cursor(void)
{
    u16 position = (u16)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)(position >> 8));
}

static void scroll_if_needed(void)
{
    if (cursor_y < VGA_HEIGHT) {
        return;
    }

    for (u32 row = 1; row < VGA_HEIGHT; row++) {
        for (u32 column = 0; column < VGA_WIDTH; column++) {
            VGA_MEMORY[(row - 1) * VGA_WIDTH + column] = VGA_MEMORY[row * VGA_WIDTH + column];
        }
    }

    for (u32 column = 0; column < VGA_WIDTH; column++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + column] = make_entry(' ', current_color);
    }

    cursor_y = VGA_HEIGHT - 1;
}

void console_set_color(u8 foreground, u8 background)
{
    current_color = make_color(foreground, background);
}

void console_clear(void)
{
    for (u32 index = 0; index < VGA_WIDTH * VGA_HEIGHT; index++) {
        VGA_MEMORY[index] = make_entry(' ', current_color);
    }

    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void console_init(void)
{
    cursor_x = 0;
    cursor_y = 0;
    current_color = make_color(15, 0);
    console_clear();
}

void console_putc(char character)
{
    serial_write_char(character);

    if (character == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (character == '\r') {
        cursor_x = 0;
    } else if (character == '\b') {
        if (cursor_x == 0 && cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        } else if (cursor_x > 0) {
            cursor_x--;
        }
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = make_entry(' ', current_color);
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = make_entry(character, current_color);
        cursor_x++;
        if (cursor_x == VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    scroll_if_needed();
    update_cursor();
}

void console_write(const char *text)
{
    while (*text != '\0') {
        console_putc(*text++);
    }
}

void console_write_u32(u32 value)
{
    char digits[10];
    u32 count = 0;

    if (value == 0) {
        console_putc('0');
        return;
    }

    while (value > 0) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }

    while (count > 0) {
        console_putc(digits[--count]);
    }
}
