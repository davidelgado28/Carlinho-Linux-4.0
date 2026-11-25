#include "console.h"
#include "keyboard.h"
#include "ports.h"
#include "types.h"

static const char keymap[128] = {
    [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5',
    [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0',
    [12] = '-', [13] = '=', [14] = '\b', [15] = '\t',
    [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't',
    [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p',
    [26] = '[', [27] = ']', [28] = '\n',
    [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g',
    [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l', [39] = ';',
    [40] = '\'', [41] = '`', [43] = '\\',
    [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b',
    [49] = 'n', [50] = 'm', [51] = ',', [52] = '.', [53] = '/',
    [57] = ' '
};

static volatile u8 shift_down;
static volatile u8 caps_lock;

static char shifted_character(char character)
{
    if (character >= 'a' && character <= 'z') {
        if ((shift_down != 0) != (caps_lock != 0)) {
            return (char)(character - 'a' + 'A');
        }
        return character;
    }

    if (shift_down == 0) {
        return character;
    }

    switch (character) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case '\\': return '|';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return character;
    }
}

void keyboard_init(void)
{
    shift_down = 0;
    caps_lock = 0;
}

void keyboard_interrupt(void)
{
    u8 scancode = inb(0x60);

    if (scancode == 42 || scancode == 54) {
        shift_down = 1;
        return;
    }

    if (scancode == 170 || scancode == 182) {
        shift_down = 0;
        return;
    }

    if ((scancode & 0x80) != 0) {
        return;
    }

    if (scancode == 58) {
        caps_lock = (u8)!caps_lock;
        return;
    }

    if (scancode < 128 && keymap[scancode] != '\0') {
        console_handle_key(shifted_character(keymap[scancode]));
    }
}
