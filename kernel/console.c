#include "console.h"
#include "fs.h"
#include "pit.h"
#include "ports.h"
#include "serial.h"
#include "shell.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile u16 *)0xB8000)
#define CELL_WIDTH 8
#define CELL_HEIGHT 12
#define APP_COUNT 4
#define APP_NOTES 0
#define APP_TERMINAL 1
#define APP_BROWSER 2
#define APP_MUSIC 3
#define TERMINAL_MAX_COLUMNS 100
#define TERMINAL_MAX_ROWS 38
#define NOTES_CAPACITY FS_CONTENT_SIZE

struct desktop_window {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    u32 restore_x;
    u32 restore_y;
    u32 restore_width;
    u32 restore_height;
    u8 open;
    u8 minimized;
    u8 maximized;
};

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
static struct desktop_window windows[APP_COUNT];
static i32 active_app = APP_TERMINAL;
static i32 dragging_app = -1;
static i32 resizing_app = -1;
static u32 drag_offset_x;
static u32 drag_offset_y;
static u32 resize_start_x;
static u32 resize_start_y;
static u32 resize_start_width;
static u32 resize_start_height;
static u8 mouse_buttons;
static u8 start_menu_open;
static u8 music_playing;
static u8 note_saved;
static char notes_text[NOTES_CAPACITY];
static u32 notes_length;
static char browser_address[128];
static u32 browser_address_length;
static char browser_message[80];
static char terminal_cells[TERMINAL_MAX_ROWS][TERMINAL_MAX_COLUMNS];
static u8 terminal_cell_colors[TERMINAL_MAX_ROWS][TERMINAL_MAX_COLUMNS];

extern const u16 desktop_wallpaper[];

static const char *app_titles[APP_COUNT] = {
    "NOTES",
    "TERMINAL",
    "CARLINHO BROWSER",
    "CARLINHO MUSICAS"
};

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

static u32 minimum(u32 first, u32 second)
{
    return first < second ? first : second;
}

static void blend_pixel(u32 x, u32 y, u8 red, u8 green, u8 blue, u8 alpha)
{
    u32 pixel = framebuffer[y * (framebuffer_pitch / 4) + x];
    u32 red_mask = (1U << framebuffer_red_size) - 1;
    u32 green_mask = (1U << framebuffer_green_size) - 1;
    u32 blue_mask = (1U << framebuffer_blue_size) - 1;
    u32 old_red = ((pixel >> framebuffer_red_position) & red_mask) * 255 / red_mask;
    u32 old_green = ((pixel >> framebuffer_green_position) & green_mask) * 255 / green_mask;
    u32 old_blue = ((pixel >> framebuffer_blue_position) & blue_mask) * 255 / blue_mask;
    red = (u8)((old_red * (255 - alpha) + (u32)red * alpha) / 255);
    green = (u8)((old_green * (255 - alpha) + (u32)green * alpha) / 255);
    blue = (u8)((old_blue * (255 - alpha) + (u32)blue * alpha) / 255);
    put_pixel(x, y, rgb_color(red, green, blue));
}

static void draw_glass(u32 x, u32 y, u32 width, u32 height, u8 alpha)
{
    if (x >= framebuffer_width || y >= framebuffer_height || width == 0 || height == 0) {
        return;
    }

    u32 end_x = minimum(framebuffer_width, x + width);
    u32 end_y = minimum(framebuffer_height, y + height);
    const u32 radius = 10;

    for (u32 py = y; py < end_y; py++) {
        for (u32 px = x; px < end_x; px++) {
            u32 dx = px < x + radius ? x + radius - px :
                px >= x + width - radius ? px - (x + width - radius - 1) : 0;
            u32 dy = py < y + radius ? y + radius - py :
                py >= y + height - radius ? py - (y + height - radius - 1) : 0;
            if (dx * dx + dy * dy <= radius * radius) {
                blend_pixel(px, py, 20, 20, 24, alpha);
            }
        }
    }
}

static void draw_circle(u32 center_x, u32 center_y, u32 radius, u32 color)
{
    for (i32 y = -(i32)radius; y <= (i32)radius; y++) {
        for (i32 x = -(i32)radius; x <= (i32)radius; x++) {
            if (x * x + y * y <= (i32)(radius * radius)) {
                i32 pixel_x = (i32)center_x + x;
                i32 pixel_y = (i32)center_y + y;
                if (pixel_x >= 0 && pixel_y >= 0) {
                    put_pixel((u32)pixel_x, (u32)pixel_y, color);
                }
            }
        }
    }
}

static void draw_icon(u32 app, u32 x, u32 y, u32 size)
{
    u32 red = rgb_color(220, 32, 54);
    u32 white = rgb_color(245, 246, 250);
    u32 black = rgb_color(22, 24, 30);
    u32 accent = app == APP_BROWSER ? rgb_color(70, 155, 255) :
        app == APP_MUSIC ? rgb_color(188, 64, 226) : rgb_color(246, 68, 81);

    draw_rectangle(x + 2, y + 2, size - 4, size - 4, accent);
    if (app == APP_NOTES) {
        draw_rectangle(x + size / 4, y + size / 6, size / 2, size * 2 / 3, white);
        draw_rectangle(x + size / 3, y + size / 3, size / 3, 2, red);
        draw_rectangle(x + size / 3, y + size / 2, size / 3, 2, red);
        draw_rectangle(x + size / 3, y + size * 2 / 3, size / 4, 2, red);
    } else if (app == APP_TERMINAL) {
        draw_rectangle(x + size / 6, y + size / 4, size * 2 / 3, size / 2, black);
        draw_text(x + size / 4, y + size / 3, ">_", white);
    } else if (app == APP_BROWSER) {
        draw_circle(x + size / 2, y + size / 2, size / 3, white);
        draw_rectangle(x + size / 5, y + size / 2 - 1, size * 3 / 5, 2, accent);
        draw_rectangle(x + size / 2 - 1, y + size / 5, 2, size * 3 / 5, accent);
    } else {
        draw_circle(x + size / 2, y + size / 2, size / 3, black);
        draw_rectangle(x + size / 2, y + size / 3, 3, size / 3, white);
        draw_circle(x + size / 2 - 3, y + size * 2 / 3, 4, white);
        draw_rectangle(x + size / 2, y + size / 3, size / 4, 3, white);
    }
    (void)red;
}

static void draw_wallpaper(void)
{
    for (u32 y = 0; y < framebuffer_height; y++) {
        u32 source_y = y * 600 / framebuffer_height;
        for (u32 x = 0; x < framebuffer_width; x++) {
            u32 source_x = x * 800 / framebuffer_width;
            u16 color = desktop_wallpaper[source_y * 800 + source_x];
            u8 red = (u8)(((color >> 11) & 0x1F) * 255 / 31);
            u8 green = (u8)(((color >> 5) & 0x3F) * 255 / 63);
            u8 blue = (u8)((color & 0x1F) * 255 / 31);
            put_pixel(x, y, rgb_color(red, green, blue));
        }
    }
}

static void draw_app_content(u32 app, const struct desktop_window *window);
static void draw_terminal_character(char character, u32 x, u32 y);

static void draw_window(u32 app)
{
    struct desktop_window *window = &windows[app];
    if (!window->open || window->minimized) {
        return;
    }

    u32 white = rgb_color(244, 245, 250);
    u32 muted = rgb_color(171, 174, 188);
    u32 border = rgb_color(84, 84, 92);
    u32 control_y = window->y + 13;

    draw_glass(window->x, window->y, window->width, window->height, 228);
    draw_rectangle(window->x, window->y + 43, window->width, 1, border);
    draw_icon(app, window->x + 14, window->y + 10, 24);
    draw_text(window->x + 48, window->y + 19, app_titles[app], white);

    draw_circle(window->x + window->width - 25, control_y, 7, rgb_color(248, 91, 94));
    draw_rectangle(window->x + window->width - 28, control_y, 6, 1, white);
    draw_circle(window->x + window->width - 51, control_y, 7, rgb_color(255, 190, 67));
    if (window->maximized) {
        draw_rectangle(window->x + window->width - 54, control_y - 3, 6, 6, white);
    } else {
        draw_rectangle(window->x + window->width - 54, control_y - 3, 6, 6, white);
    }
    draw_circle(window->x + window->width - 77, control_y, 7, rgb_color(93, 204, 117));
    draw_rectangle(window->x + window->width - 80, control_y, 6, 1, white);

    if (app == APP_TERMINAL) {
        terminal_left = window->x + 16;
        terminal_top = window->y + 54;
        terminal_columns = minimum((window->width - 32) / CELL_WIDTH, TERMINAL_MAX_COLUMNS);
        terminal_rows = minimum((window->height - 66) / CELL_HEIGHT, TERMINAL_MAX_ROWS);
        if (terminal_columns == 0) {
            terminal_columns = 1;
        }
        if (terminal_rows == 0) {
            terminal_rows = 1;
        }
        u32 unobscured_rows = terminal_top < framebuffer_height - 72 ?
            (framebuffer_height - 72 - terminal_top) / CELL_HEIGHT : 1;
        terminal_rows = minimum(terminal_rows, unobscured_rows);
        if (terminal_rows == 0) {
            terminal_rows = 1;
        }
        if (cursor_x >= terminal_columns) {
            cursor_x = terminal_columns - 1;
        }
        if (cursor_y >= terminal_rows) {
            cursor_y = terminal_rows - 1;
        }
        terminal_background = rgb_color(7, 8, 11);
        draw_rectangle(terminal_left, terminal_top, terminal_columns * CELL_WIDTH,
                       terminal_rows * CELL_HEIGHT, terminal_background);
        u8 saved_color = current_color;
        for (u32 row = 0; row < terminal_rows; row++) {
            for (u32 column = 0; column < terminal_columns; column++) {
                current_color = terminal_cell_colors[row][column];
                draw_terminal_character(terminal_cells[row][column], column, row);
            }
        }
        current_color = saved_color;
    } else {
        draw_app_content(app, window);
    }

    (void)muted;
}

static void draw_desktop_icons(void)
{
    static const char *labels[APP_COUNT] = {
        "NOTES", "TERMINAL", "BROWSER", "MUSIC"
    };
    u32 white = rgb_color(246, 246, 248);
    for (u32 app = 0; app < APP_COUNT; app++) {
        u32 y = 28 + app * 91;
        draw_icon(app, 31, y, 44);
        draw_text(17, y + 52, labels[app], white);
    }
    draw_text(31, framebuffer_height - 104, "CARLINHO OS", white);
}

static u32 taskbar_x(void)
{
    u32 width = minimum(framebuffer_width - 32, 920);
    return (framebuffer_width - width) / 2;
}

static u32 taskbar_y(void)
{
    return framebuffer_height - 72;
}

static void draw_taskbar(void)
{
    u32 width = minimum(framebuffer_width - 32, 920);
    u32 x = taskbar_x();
    u32 y = taskbar_y();
    u32 red = rgb_color(220, 35, 54);
    u32 white = rgb_color(242, 243, 247);
    u32 muted = rgb_color(185, 185, 194);
    u32 visible_apps = 0;
    u32 uptime = pit_uptime_seconds();
    u32 minutes = uptime / 60;
    u32 seconds = uptime % 60;
    char clock_text[6];

    clock_text[0] = (char)('0' + (minutes / 10) % 10);
    clock_text[1] = (char)('0' + minutes % 10);
    clock_text[2] = ':';
    clock_text[3] = (char)('0' + seconds / 10);
    clock_text[4] = (char)('0' + seconds % 10);
    clock_text[5] = '\0';

    draw_glass(x, y, width, 58, 210);
    draw_circle(x + 29, y + 29, 19, red);
    draw_text(x + 21, y + 25, "C", white);

    for (u32 app = 0; app < APP_COUNT; app++) {
        if (windows[app].open) {
            visible_apps++;
        }
    }
    u32 slot = 43;
    u32 app_x = framebuffer_width / 2 - visible_apps * slot / 2;
    for (u32 app = 0; app < APP_COUNT; app++) {
        if (!windows[app].open) {
            continue;
        }
        draw_icon(app, app_x + 5, y + 10, 34);
        if ((i32)app == active_app && !windows[app].minimized) {
            draw_rectangle(app_x + 13, y + 49, 18, 2, red);
        }
        app_x += slot;
    }

    u32 player_x = x + width - 279;
    draw_text(player_x, y + 16, music_playing ? "||  NOW PLAYING" : ">  NO TRACK", white);
    draw_text(player_x, y + 34, "CARLINHO MUSICAS", muted);
    draw_rectangle(player_x - 12, y + 10, 1, 38, rgb_color(98, 98, 107));
    draw_text(x + width - 141, y + 16, "LOCAL", muted);
    draw_text(x + width - 76, y + 15, clock_text, white);
    draw_text(x + width - 76, y + 34, "GUEST", muted);
}

static void draw_start_menu(void)
{
    if (!start_menu_open) {
        return;
    }

    u32 x = taskbar_x() + 8;
    u32 y = taskbar_y() - 230;
    u32 white = rgb_color(245, 245, 249);
    u32 muted = rgb_color(183, 184, 194);
    u32 red = rgb_color(220, 35, 54);

    draw_glass(x, y, 320, 220, 237);
    draw_text(x + 20, y + 20, "CARLINHO OS", white);
    draw_text(x + 20, y + 40, "SEU ESPACO, DO SEU JEITO", muted);
    draw_rectangle(x + 16, y + 59, 286, 27, rgb_color(47, 47, 54));
    draw_text(x + 26, y + 69, "PESQUISAR APLICATIVOS", muted);
    draw_text(x + 20, y + 101, "FIXADOS", white);
    for (u32 app = 0; app < APP_COUNT; app++) {
        u32 column = app % 2;
        u32 row = app / 2;
        u32 cell_x = x + 24 + column * 143;
        u32 cell_y = y + 122 + row * 42;
        draw_icon(app, cell_x, cell_y, 27);
        draw_text(cell_x + 36, cell_y + 10, app_titles[app], white);
    }
    draw_circle(x + 281, y + 24, 5, red);
}

static void draw_desktop(void)
{
    draw_wallpaper();
    draw_desktop_icons();

    for (u32 app = 0; app < APP_COUNT; app++) {
        if ((i32)app != active_app) {
            draw_window(app);
        }
    }
    if (active_app >= 0 && active_app < APP_COUNT) {
        draw_window((u32)active_app);
    }

    draw_taskbar();
    draw_start_menu();
}

static void load_notes(void)
{
    const struct fs_entry *entry = fs_find("/home/carlinho/notes.txt", "/");
    notes_length = 0;
    notes_text[0] = '\0';
    if (entry == 0 || entry->directory) {
        return;
    }

    notes_length = minimum(entry->size, NOTES_CAPACITY - 1);
    for (u32 index = 0; index < notes_length; index++) {
        notes_text[index] = entry->content[index];
    }
    notes_text[notes_length] = '\0';
}

static void draw_notes_content(const struct desktop_window *window)
{
    u32 white = rgb_color(242, 242, 247);
    u32 muted = rgb_color(177, 178, 188);
    u32 red = rgb_color(239, 62, 76);
    u32 toolbar_y = window->y + 54;
    u32 editor_y = window->y + 91;
    u32 editor_height = window->height - 105;
    u32 editor_width = window->width - 28;
    u32 text_x = window->x + 25;
    u32 text_y = editor_y + 12;

    draw_text(window->x + 18, toolbar_y + 5, "NOVO", white);
    draw_text(window->x + 66, toolbar_y + 5, "ABRIR", white);
    draw_text(window->x + 122, toolbar_y + 5, "SALVAR", red);
    draw_text(window->x + 194, toolbar_y + 5, "SALVAR COMO", muted);
    draw_text(window->x + window->width - 106, toolbar_y + 5,
              note_saved ? "SALVO NA RAM" : "NAO SALVO", muted);
    draw_rectangle(window->x + 14, editor_y, editor_width, editor_height,
                   rgb_color(26, 27, 33));

    u32 columns = (editor_width - 20) / CELL_WIDTH;
    u32 rows = editor_height > 20 ? (editor_height - 20) / CELL_HEIGHT : 1;
    u32 row = 0;
    u32 column = 0;
    for (u32 index = 0; index < notes_length && row < rows; index++) {
        char character = notes_text[index];
        if (character == '\n' || column >= columns) {
            row++;
            column = 0;
            if (character == '\n') {
                continue;
            }
        }
        if (row >= rows) {
            break;
        }
        char letter[2];
        letter[0] = character;
        letter[1] = '\0';
        draw_text(text_x + column * CELL_WIDTH, text_y + row * CELL_HEIGHT, letter, white);
        column++;
    }
}

static void draw_browser_content(const struct desktop_window *window)
{
    u32 white = rgb_color(243, 244, 249);
    u32 muted = rgb_color(183, 184, 194);
    u32 red = rgb_color(218, 34, 54);
    u32 toolbar_y = window->y + 53;
    u32 content_top = window->y + 101;
    u32 center_x = window->x + window->width / 2;

    draw_circle(window->x + 28, toolbar_y + 13, 9, rgb_color(74, 77, 88));
    draw_text(window->x + 25, toolbar_y + 9, "<", white);
    draw_circle(window->x + 53, toolbar_y + 13, 9, rgb_color(74, 77, 88));
    draw_text(window->x + 50, toolbar_y + 9, ">", white);
    draw_circle(window->x + 78, toolbar_y + 13, 9, rgb_color(74, 77, 88));
    draw_text(window->x + 74, toolbar_y + 9, "O", white);
    draw_rectangle(window->x + 105, toolbar_y, window->width - 124, 27,
                   rgb_color(40, 41, 49));
    draw_text(window->x + 116, toolbar_y + 9,
              browser_address_length ? browser_address : "https://carlinho.local", white);

    draw_circle(center_x, content_top + 51, 35, red);
    draw_text(center_x - 8, content_top + 43, "C", white);
    draw_text(center_x - 80, content_top + 105, "OLA, INTERNET", white);
    draw_text(center_x - 121, content_top + 130, "UM NOVO JEITO DE NAVEGAR", muted);
    draw_glass(center_x - 145, content_top + 164, 290, 40, 130);
    draw_text(center_x - 121, content_top + 181, "WIKIPEDIA     MDN     GITHUB", white);
    if (browser_message[0] != '\0') {
        draw_text(center_x - 141, content_top + 225, browser_message, red);
    } else {
        draw_text(center_x - 138, content_top + 225, "REDE NAO DISPONIVEL NESTA VERSAO", muted);
    }
}

static void draw_music_note(u32 center_x, u32 center_y)
{
    u32 white = rgb_color(246, 245, 249);
    draw_rectangle(center_x + 8, center_y - 29, 5, 40, white);
    draw_rectangle(center_x + 8, center_y - 29, 21, 5, white);
    draw_rectangle(center_x + 24, center_y - 29, 5, 28, white);
    draw_circle(center_x, center_y + 13, 10, white);
    draw_circle(center_x + 21, center_y + 1, 10, white);
}

static void draw_music_content(const struct desktop_window *window)
{
    u32 white = rgb_color(244, 244, 249);
    u32 muted = rgb_color(175, 176, 187);
    u32 red = rgb_color(219, 35, 54);
    u32 center_x = window->x + window->width / 2;
    u32 content_y = window->y + 60;

    draw_glass(center_x - 67, content_y, 134, 134, 190);
    draw_circle(center_x, content_y + 67, 45, rgb_color(36, 36, 43));
    draw_circle(center_x, content_y + 67, 15, red);
    draw_music_note(center_x - 10, content_y + 68);
    draw_text(center_x - 126, content_y + 148, "SUA TRILHA COMeca AQUI", white);
    draw_text(center_x - 96, content_y + 169, "AUDIO NAO DISPONIVEL", muted);
    draw_rectangle(window->x + 35, content_y + 202, window->width - 70, 4,
                   rgb_color(75, 75, 84));
    draw_rectangle(window->x + 35, content_y + 202,
                   music_playing ? (window->width - 70) / 3 : 2, 4, red);
    draw_circle(center_x - 34, content_y + 232, 12, rgb_color(55, 55, 63));
    draw_text(center_x - 38, content_y + 228, "|<", white);
    draw_circle(center_x, content_y + 232, 18, red);
    draw_text(center_x - 4, content_y + 228, music_playing ? "||" : ">", white);
    draw_circle(center_x + 34, content_y + 232, 12, rgb_color(55, 55, 63));
    draw_text(center_x + 30, content_y + 228, ">|", white);
    draw_text(window->x + 24, content_y + 269, "BIBLIOTECA", white);
    draw_text(window->x + 24, content_y + 291, "NENHUMA FAIXA DISPONIVEL", muted);
}

static void draw_app_content(u32 app, const struct desktop_window *window)
{
    if (app == APP_NOTES) {
        draw_notes_content(window);
    } else if (app == APP_BROWSER) {
        draw_browser_content(window);
    } else if (app == APP_MUSIC) {
        draw_music_content(window);
    }
}

static void initialize_desktop_state(void)
{
    static const u32 desired_widths[APP_COUNT] = {620, 760, 760, 620};
    static const u32 desired_heights[APP_COUNT] = {430, 500, 500, 460};
    for (u32 app = 0; app < APP_COUNT; app++) {
        struct desktop_window *window = &windows[app];
        window->width = minimum(desired_widths[app], framebuffer_width - 32);
        window->height = minimum(desired_heights[app], framebuffer_height - 90);
        window->x = 42 + app * 22;
        window->y = 38 + app * 18;
        if (window->x + window->width > framebuffer_width - 8) {
            window->x = framebuffer_width - window->width - 8;
        }
        if (window->y + window->height > framebuffer_height - 78) {
            window->y = framebuffer_height - window->height - 78;
        }
        window->restore_x = window->x;
        window->restore_y = window->y;
        window->restore_width = window->width;
        window->restore_height = window->height;
    }

    windows[APP_TERMINAL].open = 1;
    active_app = APP_TERMINAL;
    pointer_x = framebuffer_width / 2;
    pointer_y = framebuffer_height / 2;
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
    for (u32 row = 1; row < terminal_rows; row++) {
        for (u32 column = 0; column < terminal_columns; column++) {
            terminal_cells[row - 1][column] = terminal_cells[row][column];
            terminal_cell_colors[row - 1][column] = terminal_cell_colors[row][column];
        }
    }
    for (u32 column = 0; column < terminal_columns; column++) {
        terminal_cells[terminal_rows - 1][column] = '\0';
        terminal_cell_colors[terminal_rows - 1][column] = current_color;
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

static u8 point_in(u32 x, u32 y, u32 left, u32 top, u32 width, u32 height)
{
    return x >= left && y >= top && x - left < width && y - top < height;
}

static void select_next_window(void)
{
    active_app = -1;
    for (i32 app = APP_COUNT - 1; app >= 0; app--) {
        if (windows[app].open && !windows[app].minimized) {
            active_app = app;
            return;
        }
    }
}

static void open_app(u32 app)
{
    if (app >= APP_COUNT) {
        return;
    }
    if (!windows[app].open && app == APP_NOTES) {
        load_notes();
    }
    windows[app].open = 1;
    windows[app].minimized = 0;
    active_app = (i32)app;
    start_menu_open = 0;
}

static void toggle_maximize(u32 app)
{
    struct desktop_window *window = &windows[app];
    if (window->maximized) {
        window->x = window->restore_x;
        window->y = window->restore_y;
        window->width = window->restore_width;
        window->height = window->restore_height;
        window->maximized = 0;
    } else {
        window->restore_x = window->x;
        window->restore_y = window->y;
        window->restore_width = window->width;
        window->restore_height = window->height;
        window->x = 8;
        window->y = 8;
        window->width = framebuffer_width - 16;
        window->height = framebuffer_height - 88;
        window->maximized = 1;
    }
}

static void save_notes(void)
{
    notes_text[notes_length] = '\0';
    note_saved = fs_write("/notes.txt", "/", notes_text, 0) == FS_OK;
}

static void handle_window_click(u32 app, u32 x, u32 y)
{
    struct desktop_window *window = &windows[app];
    u32 relative_x = x - window->x;
    u32 relative_y = y - window->y;
    active_app = (i32)app;
    window->minimized = 0;

    if (point_in(x, y, window->x + window->width - 34, window->y + 4, 20, 20)) {
        window->open = 0;
        if (active_app == (i32)app) {
            select_next_window();
        }
        return;
    }
    if (point_in(x, y, window->x + window->width - 60, window->y + 4, 20, 20)) {
        toggle_maximize(app);
        return;
    }
    if (point_in(x, y, window->x + window->width - 86, window->y + 4, 20, 20)) {
        window->minimized = 1;
        select_next_window();
        return;
    }
    if (point_in(x, y, window->x + window->width - 20,
                 window->y + window->height - 20, 18, 18)) {
        resizing_app = (i32)app;
        resize_start_x = pointer_x;
        resize_start_y = pointer_y;
        resize_start_width = window->width;
        resize_start_height = window->height;
        return;
    }
    if (relative_y < 44) {
        dragging_app = (i32)app;
        drag_offset_x = relative_x;
        drag_offset_y = relative_y;
        return;
    }

    if (app == APP_NOTES && relative_y >= 50 && relative_y < 84) {
        if (relative_x < 58) {
            notes_length = 0;
            notes_text[0] = '\0';
            note_saved = 0;
        } else if (relative_x < 112) {
            load_notes();
            note_saved = 1;
        } else if (relative_x < 186) {
            save_notes();
        }
    } else if (app == APP_BROWSER && relative_y >= 50 && relative_y < 84 &&
               relative_x >= 100) {
        browser_address_length = 0;
        browser_address[0] = '\0';
        browser_message[0] = '\0';
    } else if (app == APP_BROWSER && relative_y >= 150) {
        browser_message[0] = 'R';
        browser_message[1] = 'E';
        browser_message[2] = 'D';
        browser_message[3] = 'E';
        browser_message[4] = ' ';
        browser_message[5] = 'N';
        browser_message[6] = 'A';
        browser_message[7] = 'O';
        browser_message[8] = ' ';
        browser_message[9] = 'D';
        browser_message[10] = 'I';
        browser_message[11] = 'S';
        browser_message[12] = 'P';
        browser_message[13] = 'O';
        browser_message[14] = 'N';
        browser_message[15] = 'I';
        browser_message[16] = 'V';
        browser_message[17] = 'E';
        browser_message[18] = 'L';
        browser_message[19] = '\0';
    } else if (app == APP_MUSIC && relative_y >= 275 && relative_y < 320) {
        music_playing = !music_playing;
    }
}

static void handle_desktop_click(u32 x, u32 y)
{
    u32 menu_x = taskbar_x() + 8;
    u32 menu_y = taskbar_y() - 230;
    if (start_menu_open && point_in(x, y, menu_x, menu_y, 320, 220)) {
        if (y >= menu_y + 116) {
            u32 row = (y - (menu_y + 122)) / 42;
            u32 column = x >= menu_x + 167;
            u32 app = row * 2 + column;
            if (app < APP_COUNT) {
                open_app(app);
            }
        } else if (y < menu_y + 59) {
            open_app(APP_TERMINAL);
        }
        return;
    }

    if (point_in(x, y, taskbar_x() + 5, taskbar_y() + 5, 48, 48)) {
        start_menu_open = !start_menu_open;
        return;
    }

    u32 visible_apps = 0;
    for (u32 app = 0; app < APP_COUNT; app++) {
        if (windows[app].open) {
            visible_apps++;
        }
    }
    u32 app_x = framebuffer_width / 2 - visible_apps * 43 / 2;
    for (u32 app = 0; app < APP_COUNT; app++) {
        if (!windows[app].open) {
            continue;
        }
        if (point_in(x, y, app_x + 5, taskbar_y() + 8, 34, 40)) {
            if ((i32)app == active_app && !windows[app].minimized) {
                windows[app].minimized = 1;
                select_next_window();
            } else {
                open_app(app);
            }
            return;
        }
        app_x += 43;
    }

    u32 player_x = taskbar_x() + minimum(framebuffer_width - 32, 920) - 279;
    if (point_in(x, y, player_x - 8, taskbar_y() + 5, 145, 48)) {
        open_app(APP_MUSIC);
        return;
    }

    if (start_menu_open) {
        start_menu_open = 0;
    }

    if (active_app >= 0 && active_app < APP_COUNT &&
        windows[active_app].open && !windows[active_app].minimized &&
        point_in(x, y, windows[active_app].x, windows[active_app].y,
                 windows[active_app].width, windows[active_app].height)) {
        handle_window_click((u32)active_app, x, y);
        return;
    }
    for (i32 app = APP_COUNT - 1; app >= 0; app--) {
        if (app == active_app) {
            continue;
        }
        if (windows[app].open && !windows[app].minimized &&
            point_in(x, y, windows[app].x, windows[app].y,
                     windows[app].width, windows[app].height)) {
            handle_window_click((u32)app, x, y);
            return;
        }
    }

    if (x < 100 && y >= 20 && y < 28 + APP_COUNT * 91) {
        u32 app = (y - 28) / 91;
        if (app < APP_COUNT) {
            open_app(app);
        }
        return;
    }

    active_app = -1;
}

void console_mouse_event(i32 delta_x, i32 delta_y, u8 buttons)
{
    if (!graphical) {
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

    if ((buttons & 1) != 0 && dragging_app >= 0) {
        struct desktop_window *window = &windows[dragging_app];
        window->x = pointer_x > drag_offset_x ? pointer_x - drag_offset_x : 0;
        window->y = pointer_y > drag_offset_y ? pointer_y - drag_offset_y : 0;
        if (window->x + window->width > framebuffer_width) {
            window->x = framebuffer_width - window->width;
        }
        if (window->y + window->height > framebuffer_height - 10) {
            window->y = framebuffer_height - window->height - 10;
        }
        draw_desktop();
    } else if ((buttons & 1) != 0 && resizing_app >= 0) {
        struct desktop_window *window = &windows[resizing_app];
        i32 width = (i32)resize_start_width + (i32)pointer_x - (i32)resize_start_x;
        i32 height = (i32)resize_start_height + (i32)pointer_y - (i32)resize_start_y;
        if (width < 380) {
            width = 380;
        }
        if (height < 280) {
            height = 280;
        }
        window->width = minimum((u32)width, framebuffer_width - window->x);
        window->height = minimum((u32)height, framebuffer_height - window->y - 72);
        draw_desktop();
    }

    if ((buttons & 1) != 0 && (mouse_buttons & 1) == 0) {
        handle_desktop_click(pointer_x, pointer_y);
        draw_desktop();
    }
    if ((buttons & 1) == 0 && (mouse_buttons & 1) != 0) {
        dragging_app = -1;
        resizing_app = -1;
    }
    mouse_buttons = buttons;
    show_pointer();
}

void console_handle_key(char character)
{
    if (active_app < 0 || active_app >= APP_COUNT ||
        !windows[active_app].open || windows[active_app].minimized) {
        return;
    }

    if (active_app == APP_TERMINAL) {
        shell_handle_key(character);
        return;
    }

    if (active_app == APP_NOTES) {
        if (character == '\b') {
            if (notes_length > 0) {
                notes_text[--notes_length] = '\0';
            }
        } else if (character == '\n') {
            if (notes_length < NOTES_CAPACITY - 1) {
                notes_text[notes_length++] = '\n';
                notes_text[notes_length] = '\0';
            }
        } else if (character >= 32 && character <= 126 &&
                   notes_length < NOTES_CAPACITY - 1) {
            notes_text[notes_length++] = character;
            notes_text[notes_length] = '\0';
        }
        note_saved = 0;
    } else if (active_app == APP_BROWSER) {
        if (character == '\b') {
            if (browser_address_length > 0) {
                browser_address[--browser_address_length] = '\0';
            }
        } else if (character == '\n') {
            const char message[] = "REDE NAO DISPONIVEL";
            for (u32 i = 0; i < sizeof(message); i++) {
                browser_message[i] = message[i];
            }
        } else if (character >= 32 && character <= 126 &&
                   browser_address_length < sizeof(browser_address) - 1) {
            browser_address[browser_address_length++] = character;
            browser_address[browser_address_length] = '\0';
        }
    } else if (active_app == APP_MUSIC && character == ' ') {
        music_playing = !music_playing;
    }

    if (graphical && active_app >= 0) {
        hide_pointer();
        draw_window((u32)active_app);
        draw_taskbar();
        draw_start_menu();
        show_pointer();
    }
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
        for (u32 row = 0; row < TERMINAL_MAX_ROWS; row++) {
            for (u32 column = 0; column < TERMINAL_MAX_COLUMNS; column++) {
                terminal_cells[row][column] = '\0';
                terminal_cell_colors[row][column] = current_color;
            }
        }
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
        initialize_desktop_state();
        draw_desktop();
        show_pointer();
        return;
    }

    windows[APP_TERMINAL].open = 1;
    active_app = APP_TERMINAL;
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
            terminal_cells[cursor_y][cursor_x] = ' ';
            terminal_cell_colors[cursor_y][cursor_x] = current_color;
            draw_terminal_character(' ', cursor_x, cursor_y);
        } else {
            terminal_cells[cursor_y][cursor_x] = character;
            terminal_cell_colors[cursor_y][cursor_x] = current_color;
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
