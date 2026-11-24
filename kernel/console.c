#include "console.h"
#include "ports.h"
#include "serial.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile u16 *)0xB8000)
#define CELL_WIDTH 8
#define CELL_HEIGHT 12

struct multiboot_tag {
    u32 type;
    u32 size;
} PACKED;

struct multiboot_framebuffer_tag {
    u32 type;
    u32 size;
    u64 address;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bits_per_pixel;
    u8 framebuffer_type;
    u16 reserved;
    u8 red_position;
    u8 red_size;
    u8 green_position;
    u8 green_size;
    u8 blue_position;
    u8 blue_size;
} PACKED;

static const u8 font[36][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}
};

static const u32 colors[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static volatile u32 *framebuffer;
static u32 framebuffer_pitch;
static u32 framebuffer_width;
static u32 framebuffer_height;
static u8 framebuffer_red_position;
static u8 framebuffer_red_size;
static u8 framebuffer_green_position;
static u8 framebuffer_green_size;
static u8 framebuffer_blue_position;
static u8 framebuffer_blue_size;
static u8 graphical;
static u32 terminal_columns;
static u32 terminal_rows;
static u32 terminal_left;
static u32 terminal_top;
static u32 terminal_background;
static u32 cursor_x;
static u32 cursor_y;
static u8 current_color;
static u8 pointer_visible;
static u32 pointer_x;
static u32 pointer_y;
static u32 pointer_background[16][16];

static const u16 pointer_shape[16] = {
    0x8000, 0xC000, 0xE000, 0xF000,
    0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFFC0, 0xF800, 0xD800,
    0x8C00, 0x0C00, 0x0600, 0x0600
};

static u32 rgb_color(u8 red, u8 green, u8 blue)
{
    u32 r = ((u32)red >> (8 - framebuffer_red_size)) << framebuffer_red_position;
    u32 g = ((u32)green >> (8 - framebuffer_green_size)) << framebuffer_green_position;
    u32 b = ((u32)blue >> (8 - framebuffer_blue_size)) << framebuffer_blue_position;
    return r | g | b;
}

static u32 palette_color(u8 index)
{
    u32 color = colors[index & 0x0F];
    return rgb_color((u8)(color >> 16), (u8)(color >> 8), (u8)color);
}

static void put_pixel(u32 x, u32 y, u32 color)
{
    if (x < framebuffer_width && y < framebuffer_height) {
        framebuffer[y * (framebuffer_pitch / 4) + x] = color;
    }
}

static void draw_rectangle(u32 x, u32 y, u32 width, u32 height, u32 color)
{
    if (x >= framebuffer_width || y >= framebuffer_height) {
        return;
    }
    if (width > framebuffer_width - x) {
        width = framebuffer_width - x;
    }
    if (height > framebuffer_height - y) {
        height = framebuffer_height - y;
    }

    for (u32 row = 0; row < height; row++) {
        volatile u32 *destination = framebuffer + (y + row) * (framebuffer_pitch / 4) + x;
        for (u32 column = 0; column < width; column++) {
            destination[column] = color;
        }
    }
}

static u8 glyph_row(char character, u32 row)
{
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }

    u32 index;
    if (character >= 'A' && character <= 'Z') {
        index = (u32)(character - 'A');
        return font[index][row];
    }
    if (character >= '0' && character <= '9') {
        index = 26 + (u32)(character - '0');
        return font[index][row];
    }

    switch (character) {
        case '.': return row == 5 || row == 6 ? 0x0C : 0;
        case ',': return row == 5 ? 0x0C : row == 6 ? 0x08 : 0;
        case ':': return row == 1 || row == 5 ? 0x0C : 0;
        case ';': return row == 1 ? 0x0C : row == 5 ? 0x0C : row == 6 ? 0x08 : 0;
        case '-': return row == 3 ? 0x1F : 0;
        case '_': return row == 6 ? 0x1F : 0;
        case '=': return row == 2 || row == 4 ? 0x1F : 0;
        case '/': return (u8)(1U << (row < 5 ? 4 - row : 0));
        case '\\': return (u8)(1U << (row < 5 ? row : 4));
        case '!': return row < 5 ? 0x04 : row == 6 ? 0x04 : 0;
        case '?': return row == 0 ? 0x0E : row == 1 ? 0x11 : row == 2 ? 0x01 : row == 3 ? 0x06 : row == 4 ? 0x04 : row == 6 ? 0x04 : 0;
        case '+': return row == 3 ? 0x1F : row >= 1 && row <= 5 ? 0x04 : 0;
        case '*': return row == 2 || row == 4 ? 0x0A : row == 3 ? 0x04 : 0;
        case '(': return row == 0 || row == 6 ? 0x02 : row == 1 || row == 5 ? 0x04 : 0x08;
        case ')': return row == 0 || row == 6 ? 0x08 : row == 1 || row == 5 ? 0x04 : 0x02;
        case '\'': return row < 2 ? 0x04 : 0;
        case '"': return row < 2 ? 0x0A : 0;
        case '<': return row == 0 || row == 6 ? 0x02 : row == 1 || row == 5 ? 0x04 : 0x08;
        case '>': return row == 0 || row == 6 ? 0x08 : row == 1 || row == 5 ? 0x04 : 0x02;
        case '|': return 0x04;
        case '#': return row == 2 || row == 4 ? 0x1F : 0x0A;
        case ' ': return 0;
        default: return 0x1F;
    }
}

static void draw_text(u32 x, u32 y, const char *text, u32 color)
{
    while (*text != '\0') {
        char character = *text++;
        if (character == ' ') {
            x += CELL_WIDTH;
            continue;
        }

        for (u32 row = 0; row < 7; row++) {
            u8 bits = glyph_row(character, row);
            for (u32 column = 0; column < 5; column++) {
                if ((bits & (1U << (4 - column))) != 0) {
                    put_pixel(x + column + 1, y + row + 1, color);
                }
            }
        }
        x += CELL_WIDTH;
    }
}

static void draw_desktop(void)
{
    u32 dark = rgb_color(18, 30, 55);
    u32 panel = rgb_color(18, 24, 38);
    u32 window = rgb_color(21, 25, 34);
    u32 accent = rgb_color(44, 115, 190);
    u32 white = rgb_color(240, 244, 250);
    u32 muted = rgb_color(155, 175, 198);
    u32 title_x = framebuffer_width / 2 - 200;
    u32 title_y = 58;
    u32 window_width = framebuffer_width - 80;
    u32 window_height = framebuffer_height - 128;
    u32 window_x = 40;
    u32 window_y = 78;

    for (u32 y = 0; y < framebuffer_height; y++) {
        u8 blend = (u8)(y * 28 / framebuffer_height);
        draw_rectangle(0, y, framebuffer_width, 1,
                       rgb_color((u8)(18 + blend), (u8)(30 + blend), (u8)(55 + blend)));
    }

    draw_rectangle(0, 0, framebuffer_width, 34, panel);
    draw_text(14, 11, "CARLINHO OS", white);
    draw_text(framebuffer_width - 190, 11, "32 BIT   GUEST", muted);
    draw_text(title_x, title_y, "WELCOME TO YOUR DESKTOP", white);

    draw_rectangle(window_x + 3, window_y + 4, window_width, window_height, dark);
    draw_rectangle(window_x, window_y, window_width, window_height, window);
    draw_rectangle(window_x, window_y, window_width, 28, accent);
    draw_text(window_x + 12, window_y + 9, "TERMINAL", white);
    draw_rectangle(window_x + window_width - 58, window_y + 8, 10, 10, rgb_color(255, 95, 86));
    draw_rectangle(window_x + window_width - 39, window_y + 8, 10, 10, rgb_color(255, 189, 46));
    draw_rectangle(window_x + window_width - 20, window_y + 8, 10, 10, rgb_color(39, 201, 63));
    draw_rectangle(window_x + 1, window_y + 29, window_width - 2, window_height - 30, window);

    draw_rectangle(0, framebuffer_height - 44, framebuffer_width, 44, panel);
    draw_rectangle(18, framebuffer_height - 36, 28, 28, accent);
    draw_text(56, framebuffer_height - 27, "TERMINAL", white);
    draw_text(framebuffer_width - 112, framebuffer_height - 27, "CARLINHO", muted);

    terminal_left = window_x + 14;
    terminal_top = window_y + 42;
    terminal_columns = (window_width - 28) / CELL_WIDTH;
    terminal_rows = (window_height - 56) / CELL_HEIGHT;
    if (terminal_columns > 100) {
        terminal_columns = 100;
    }
    if (terminal_rows > 38) {
        terminal_rows = 38;
    }
    if (terminal_columns == 0) {
        terminal_columns = 1;
    }
    if (terminal_rows == 0) {
        terminal_rows = 1;
    }
    terminal_background = window;
}

static u8 initialize_framebuffer(u32 information)
{
    if (information == 0) {
        return 0;
    }

    const u32 *info = (const u32 *)information;
    u32 total_size = info[0];
    if (total_size < 16 || total_size > 0x100000) {
        return 0;
    }

    u32 offset = 8;
    while (offset + sizeof(struct multiboot_tag) <= total_size) {
        const struct multiboot_tag *tag = (const struct multiboot_tag *)(information + offset);
        if (tag->size < sizeof(struct multiboot_tag) || tag->size > total_size - offset) {
            return 0;
        }
        if (tag->type == 0) {
            break;
        }
        if (tag->type == 8 && tag->size >= sizeof(struct multiboot_framebuffer_tag)) {
            const struct multiboot_framebuffer_tag *frame =
                (const struct multiboot_framebuffer_tag *)tag;
            if (frame->address == 0 || (u32)(frame->address >> 32) != 0 || frame->bits_per_pixel != 32 ||
                frame->framebuffer_type != 1 || frame->width < 640 ||
                frame->width > 4096 || frame->height < 480 || frame->height > 2160 ||
                frame->pitch < frame->width * 4 ||
                frame->red_size == 0 || frame->red_size > 8 || frame->red_position + frame->red_size > 32 ||
                frame->green_size == 0 || frame->green_size > 8 || frame->green_position + frame->green_size > 32 ||
                frame->blue_size == 0 || frame->blue_size > 8 || frame->blue_position + frame->blue_size > 32) {
                return 0;
            }

            framebuffer = (volatile u32 *)(u32)frame->address;
            framebuffer_pitch = frame->pitch;
            framebuffer_width = frame->width;
            framebuffer_height = frame->height;
            framebuffer_red_position = frame->red_position;
            framebuffer_red_size = frame->red_size;
            framebuffer_green_position = frame->green_position;
            framebuffer_green_size = frame->green_size;
            framebuffer_blue_position = frame->blue_position;
            framebuffer_blue_size = frame->blue_size;
            return 1;
        }
        offset += (tag->size + 7) & ~7U;
    }

    return 0;
}

static u16 make_entry(char character, u8 color)
{
    return (u16)(u8)character | ((u16)color << 8);
}

static void update_vga_cursor(void)
{
    u16 position = (u16)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)(position >> 8));
}

static void draw_terminal_character(char character, u32 x, u32 y)
{
    u32 foreground = palette_color(current_color & 0x0F);
    u32 background = palette_color((current_color >> 4) & 0x0F);
    u32 pixel_x = terminal_left + x * CELL_WIDTH;
    u32 pixel_y = terminal_top + y * CELL_HEIGHT;

    draw_rectangle(pixel_x, pixel_y, CELL_WIDTH, CELL_HEIGHT, background);
    if (character == ' ' || character == '\0') {
        return;
    }

    for (u32 row = 0; row < 7; row++) {
        u8 bits = glyph_row(character, row);
        for (u32 column = 0; column < 5; column++) {
            if ((bits & (1U << (4 - column))) != 0) {
                put_pixel(pixel_x + column + 1, pixel_y + row + 2, foreground);
            }
        }
    }
}

static void scroll_graphical_terminal(void)
{
    u32 width = terminal_columns * CELL_WIDTH;
    u32 height = terminal_rows * CELL_HEIGHT;
    u32 pitch_pixels = framebuffer_pitch / 4;

    for (u32 y = CELL_HEIGHT; y < height; y++) {
        volatile u32 *source = framebuffer + (terminal_top + y) * pitch_pixels + terminal_left;
        volatile u32 *destination = framebuffer + (terminal_top + y - CELL_HEIGHT) * pitch_pixels + terminal_left;
        for (u32 x = 0; x < width; x++) {
            destination[x] = source[x];
        }
    }
    draw_rectangle(terminal_left, terminal_top + height - CELL_HEIGHT,
                   width, CELL_HEIGHT, terminal_background);
    cursor_y = terminal_rows - 1;
}

static void hide_pointer(void)
{
    if (!pointer_visible) {
        return;
    }
    for (u32 y = 0; y < 16; y++) {
        for (u32 x = 0; x < 16; x++) {
            put_pixel(pointer_x + x, pointer_y + y, pointer_background[y][x]);
        }
    }
    pointer_visible = 0;
}

static void show_pointer(void)
{
    if (!graphical) {
        return;
    }
    u32 black = rgb_color(0, 0, 0);
    u32 white = rgb_color(255, 255, 255);

    for (u32 y = 0; y < 16; y++) {
        for (u32 x = 0; x < 16; x++) {
            pointer_background[y][x] = framebuffer[(pointer_y + y) * (framebuffer_pitch / 4) + pointer_x + x];
        }
    }

    for (u32 y = 0; y < 16; y++) {
        for (u32 x = 0; x < 16; x++) {
            u16 mask = (u16)(1U << (15 - x));
            if ((pointer_shape[y] & mask) == 0) {
                continue;
            }

            u8 inner = x > 0 && x < 15 && y > 0 && y < 15 &&
                (pointer_shape[y] & (u16)(mask << 1)) != 0 &&
                (pointer_shape[y] & (u16)(mask >> 1)) != 0 &&
                (pointer_shape[y - 1] & mask) != 0 &&
                (pointer_shape[y + 1] & mask) != 0;
            put_pixel(pointer_x + x, pointer_y + y, inner ? white : black);
        }
    }
    pointer_visible = 1;
}

void console_set_color(u8 foreground, u8 background)
{
    current_color = (u8)((foreground & 0x0F) | ((background & 0x0F) << 4));
}

u8 console_is_graphical(void)
{
    return graphical;
}

void console_clear(void)
{
    hide_pointer();
    cursor_x = 0;
    cursor_y = 0;

    if (graphical) {
        draw_rectangle(terminal_left, terminal_top,
                       terminal_columns * CELL_WIDTH,
                       terminal_rows * CELL_HEIGHT,
                       terminal_background);
        show_pointer();
        return;
    }

    for (u32 index = 0; index < VGA_WIDTH * VGA_HEIGHT; index++) {
        VGA_MEMORY[index] = make_entry(' ', current_color);
    }
    update_vga_cursor();
}

void console_init(u32 multiboot_info)
{
    cursor_x = 0;
    cursor_y = 0;
    current_color = 0x0F;
    graphical = initialize_framebuffer(multiboot_info);

    if (graphical) {
        draw_desktop();
        return;
    }

    for (u32 index = 0; index < VGA_WIDTH * VGA_HEIGHT; index++) {
        VGA_MEMORY[index] = make_entry(' ', current_color);
    }
    update_vga_cursor();
}

void console_putc(char character)
{
    if (graphical) {
        hide_pointer();
    }
    serial_write_char(character);

    if (graphical) {
        if (character == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else if (character == '\r') {
            cursor_x = 0;
        } else if (character == '\b') {
            if (cursor_x == 0 && cursor_y > 0) {
                cursor_y--;
                cursor_x = terminal_columns - 1;
            } else if (cursor_x > 0) {
                cursor_x--;
            }
            draw_terminal_character(' ', cursor_x, cursor_y);
        } else {
            draw_terminal_character(character, cursor_x, cursor_y);
            cursor_x++;
            if (cursor_x >= terminal_columns) {
                cursor_x = 0;
                cursor_y++;
            }
        }

        if (cursor_y >= terminal_rows) {
            scroll_graphical_terminal();
        }
        show_pointer();
        return;
    }

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

    if (cursor_y >= VGA_HEIGHT) {
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
    update_vga_cursor();
}

void console_mouse_move(i32 delta_x, i32 delta_y)
{
    if (!graphical || (delta_x == 0 && delta_y == 0)) {
        return;
    }

    hide_pointer();
    i32 next_x = (i32)pointer_x + delta_x;
    i32 next_y = (i32)pointer_y - delta_y;
    if (next_x < 0) {
        next_x = 0;
    } else if ((u32)next_x > framebuffer_width - 16) {
        next_x = (i32)(framebuffer_width - 16);
    }
    if (next_y < 0) {
        next_y = 0;
    } else if ((u32)next_y > framebuffer_height - 16) {
        next_y = (i32)(framebuffer_height - 16);
    }
    pointer_x = (u32)next_x;
    pointer_y = (u32)next_y;
    show_pointer();
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
