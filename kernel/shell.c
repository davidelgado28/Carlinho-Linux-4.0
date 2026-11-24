#include "console.h"
#include "pit.h"
#include "ports.h"
#include "shell.h"
#include "string.h"

#define INPUT_SIZE 128

static char input[INPUT_SIZE];
static u32 input_length;

static void prompt(void)
{
    console_set_color(10, 0);
    console_write("carlinho");
    console_set_color(15, 0);
    console_write("> ");
}

static void execute_command(char *line)
{
    while (*line == ' ') {
        line++;
    }

    char *argument = line;
    while (*argument != '\0' && *argument != ' ') {
        argument++;
    }

    if (*argument != '\0') {
        *argument++ = '\0';
        while (*argument == ' ') {
            argument++;
        }
    }

    if (strcmp(line, "help") == 0) {
        console_write("Comandos: help, about, clear, echo, uptime, reboot, halt\n");
    } else if (strcmp(line, "about") == 0) {
        console_write("Carlinho-OS 0.1.0 - kernel freestanding para x86 de 32 bits.\n");
    } else if (strcmp(line, "clear") == 0) {
        console_clear();
    } else if (strcmp(line, "echo") == 0) {
        console_write(argument);
        console_putc('\n');
    } else if (strcmp(line, "uptime") == 0) {
        console_write("Ligado ha ");
        console_write_u32(pit_uptime_seconds());
        console_write(" segundos.\n");
    } else if (strcmp(line, "reboot") == 0) {
        interrupts_disable();
        while ((inb(0x64) & 0x02) != 0) {
        }
        outb(0x64, 0xFE);
        for (;;) {
            __asm__ volatile ("hlt");
        }
    } else if (strcmp(line, "halt") == 0) {
        interrupts_disable();
        console_write("Sistema desligado.\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    } else if (*line != '\0') {
        console_write("Comando desconhecido: ");
        console_write(line);
        console_write(". Digite help.\n");
    }
}

void shell_init(void)
{
    input_length = 0;
    console_write("Digite help para ver os comandos.\n");
    prompt();
}

void shell_handle_key(char character)
{
    if (character == '\n') {
        console_putc('\n');
        input[input_length] = '\0';
        execute_command(input);
        input_length = 0;
        prompt();
        return;
    }

    if (character == '\b') {
        if (input_length > 0) {
            input_length--;
            console_putc('\b');
        }
        return;
    }

    if (character >= ' ' && character <= '~' && input_length < INPUT_SIZE - 1) {
        input[input_length++] = character;
        console_putc(character);
    }
}
