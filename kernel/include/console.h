#ifndef CARLINHO_CONSOLE_H
#define CARLINHO_CONSOLE_H

#include "types.h"

void console_init(void);
void console_clear(void);
void console_putc(char character);
void console_write(const char *text);
void console_write_u32(u32 value);
void console_set_color(u8 foreground, u8 background);

#endif
