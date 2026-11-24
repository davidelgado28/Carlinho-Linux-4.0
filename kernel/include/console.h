#ifndef CARLINHO_CONSOLE_H
#define CARLINHO_CONSOLE_H

#include "types.h"

void console_init(u32 multiboot_info);
void console_clear(void);
void console_putc(char character);
void console_write(const char *text);
void console_write_u32(u32 value);
void console_set_color(u8 foreground, u8 background);
u8 console_is_graphical(void);
void console_mouse_move(i32 delta_x, i32 delta_y);

#endif
