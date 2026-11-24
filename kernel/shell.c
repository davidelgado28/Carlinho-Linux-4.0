#include "console.h"
#include "fs.h"
#include "pit.h"
#include "ports.h"
#include "shell.h"
#include "string.h"

#define INPUT_SIZE 256
#define ARGUMENT_COUNT 20
#define HISTORY_SIZE 8

static char input[INPUT_SIZE];
static char working_directory[FS_PATH_SIZE] = "/home/carlinho";
static char history[HISTORY_SIZE][INPUT_SIZE];
static u32 history_count;
static u32 history_next;
static u32 input_length;

static char redirect_write[] = ">";
static char redirect_append[] = ">>";
static char pipe_operator[] = "|";

static u8 is_space(char character)
{
    return character == ' ' || character == '\t';
}

static u32 parse_number(const char *text, u32 fallback)
{
    if (*text == '\0') {
        return fallback;
    }

    u32 value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (u32)(*text - '0');
        text++;
    }
    return *text == '\0' ? value : fallback;
}

static u32 tokenize(char *line, char **arguments, u32 capacity)
{
    u32 count = 0;
    char *cursor = line;

    while (*cursor != '\0') {
        while (is_space(*cursor)) {
            *cursor++ = '\0';
        }
        if (*cursor == '\0') {
            break;
        }

        if (count >= capacity) {
            return 0;
        }

        if (*cursor == '|') {
            arguments[count++] = pipe_operator;
            cursor++;
            continue;
        }
        if (*cursor == '>') {
            if (cursor[1] == '>') {
                arguments[count++] = redirect_append;
                cursor += 2;
            } else {
                arguments[count++] = redirect_write;
                cursor++;
            }
            continue;
        }

        char quote = '\0';
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor++;
        }
        arguments[count++] = cursor;

        while (*cursor != '\0' &&
               (quote != '\0' ? *cursor != quote : !is_space(*cursor) && *cursor != '>' && *cursor != '|')) {
            cursor++;
        }

        if (quote != '\0') {
            if (*cursor == quote) {
                *cursor++ = '\0';
            }
        } else if (*cursor != '\0' && is_space(*cursor)) {
            *cursor++ = '\0';
        }
    }

    return count;
}

static void print_fs_error(u32 error, const char *path)
{
    console_write("carlinho: ");
    switch (error) {
        case FS_NOT_FOUND:
            console_write("nao encontrado: ");
            console_write(path);
            break;
        case FS_EXISTS:
            console_write("ja existe: ");
            console_write(path);
            break;
        case FS_NOT_DIRECTORY:
            console_write("nao e um diretorio: ");
            console_write(path);
            break;
        case FS_NOT_EMPTY:
            console_write("diretorio nao vazio: ");
            console_write(path);
            break;
        case FS_NO_SPACE:
            console_write("memoria do sistema de arquivos esgotada");
            break;
        case FS_IS_DIRECTORY:
            console_write("e um diretorio: ");
            console_write(path);
            break;
        default:
            console_write("caminho invalido: ");
            console_write(path);
            break;
    }
    console_putc('\n');
}

static const char *base_name(const char *path)
{
    const char *base = path;
    while (*path != '\0') {
        if (*path == '/') {
            base = path + 1;
        }
        path++;
    }
    return base;
}

static u8 join_path(char *output, const char *directory, const char *name)
{
    u32 length = 0;
    while (directory[length] != '\0' && length + 1 < FS_PATH_SIZE) {
        output[length] = directory[length];
        length++;
    }
    if (directory[length] != '\0') {
        return 0;
    }

    if (length > 1 && output[length - 1] != '/') {
        if (length + 1 >= FS_PATH_SIZE) {
            return 0;
        }
        output[length++] = '/';
    }

    u32 name_index = 0;
    while (name[name_index] != '\0' && length + 1 < FS_PATH_SIZE) {
        output[length++] = name[name_index++];
    }
    if (name[name_index] != '\0') {
        return 0;
    }
    output[length] = '\0';
    return 1;
}

static void command_ls(char **arguments, u32 count)
{
    const char *path = ".";
    u8 show_hidden = 0;
    u8 long_format = 0;

    for (u32 index = 1; index < count; index++) {
        if (arguments[index][0] == '-' && arguments[index][1] != '\0') {
            for (u32 option = 1; arguments[index][option] != '\0'; option++) {
                if (arguments[index][option] == 'a') {
                    show_hidden = 1;
                } else if (arguments[index][option] == 'l') {
                    long_format = 1;
                } else {
                    console_write("ls: opcao desconhecida\n");
                    return;
                }
            }
        } else {
            path = arguments[index];
        }
    }

    const struct fs_entry *target = fs_find(path, working_directory);
    if (target == 0) {
        char normalized[FS_PATH_SIZE];
        if (fs_resolve(path, working_directory, normalized) == FS_OK) {
            print_fs_error(FS_NOT_FOUND, normalized);
        } else {
            print_fs_error(FS_INVALID, path);
        }
        return;
    }

    if (!target->directory) {
        if (long_format) {
            console_write_u32(target->size);
            console_write("  ");
        }
        console_write(base_name(target->path));
        console_putc('\n');
        return;
    }

    const struct fs_entry *items[FS_ENTRY_COUNT];
    u32 item_count = 0;
    u32 result = fs_list(path, working_directory, items, FS_ENTRY_COUNT, &item_count);
    if (result != FS_OK) {
        print_fs_error(result, path);
        return;
    }

    if (show_hidden) {
        console_write(".  ..\n");
    }
    for (u32 index = 0; index < item_count; index++) {
        const char *name = base_name(items[index]->path);
        if (!show_hidden && name[0] == '.') {
            continue;
        }
        if (long_format) {
            console_write(items[index]->directory ? "dir  " : "file ");
            console_write_u32(items[index]->size);
            console_write("  ");
        }
        console_write(name);
        if (items[index]->directory) {
            console_putc('/');
        }
        console_putc('\n');
    }
}

static void command_echo(char **arguments, u32 count)
{
    char output[FS_CONTENT_SIZE];
    u32 length = 0;
    u32 redirect_index = count;

    for (u32 index = 1; index < count; index++) {
        if (arguments[index] == redirect_write || arguments[index] == redirect_append) {
            redirect_index = index;
            break;
        }
    }

    for (u32 index = 1; index < redirect_index; index++) {
        if (index > 1 && length + 1 < sizeof(output)) {
            output[length++] = ' ';
        }
        for (u32 character = 0; arguments[index][character] != '\0'; character++) {
            if (length + 1 >= sizeof(output)) {
                console_write("echo: texto muito longo\n");
                return;
            }
            output[length++] = arguments[index][character];
        }
    }
    output[length] = '\0';

    if (redirect_index < count) {
        if (redirect_index + 1 >= count || redirect_index + 2 != count) {
            console_write("Uso: echo TEXTO > ARQUIVO\n");
            return;
        }
        u32 result = fs_write(arguments[redirect_index + 1], working_directory, output,
                              arguments[redirect_index] == redirect_append);
        if (result != FS_OK) {
            print_fs_error(result, arguments[redirect_index + 1]);
        }
        return;
    }

    console_write(output);
    console_putc('\n');
}

static void command_cat(char **arguments, u32 count)
{
    if (count < 2) {
        console_write("Uso: cat ARQUIVO...\n");
        return;
    }
    for (u32 index = 1; index < count; index++) {
        const struct fs_entry *entry = fs_find(arguments[index], working_directory);
        if (entry == 0) {
            print_fs_error(FS_NOT_FOUND, arguments[index]);
        } else if (entry->directory) {
            print_fs_error(FS_IS_DIRECTORY, arguments[index]);
        } else {
            console_write(entry->content);
        }
    }
}

static void command_mkdir(char **arguments, u32 count)
{
    u8 parents = 0;
    u32 first_path = 1;

    if (count > 1 && strcmp(arguments[1], "-p") == 0) {
        parents = 1;
        first_path = 2;
    }
    if (first_path >= count) {
        console_write("Uso: mkdir [-p] DIRETORIO...\n");
        return;
    }

    for (u32 index = first_path; index < count; index++) {
        if (!parents) {
            u32 result = fs_create(arguments[index], working_directory, 1);
            if (result != FS_OK) {
                print_fs_error(result, arguments[index]);
            }
            continue;
        }

        char normalized[FS_PATH_SIZE];
        if (fs_resolve(arguments[index], working_directory, normalized) != FS_OK) {
            print_fs_error(FS_INVALID, arguments[index]);
            continue;
        }

        for (u32 cursor = 1; ; cursor++) {
            if (normalized[cursor] != '/' && normalized[cursor] != '\0') {
                continue;
            }
            char saved = normalized[cursor];
            normalized[cursor] = '\0';
            const struct fs_entry *existing = fs_find(normalized, "/");
            u32 result = existing != 0
                ? (existing->directory ? FS_OK : FS_NOT_DIRECTORY)
                : fs_create(normalized, "/", 1);
            normalized[cursor] = saved;
            if (result != FS_OK) {
                print_fs_error(result, normalized);
                break;
            }
            if (saved == '\0') {
                break;
            }
        }
    }
}

static void command_touch(char **arguments, u32 count)
{
    if (count < 2) {
        console_write("Uso: touch ARQUIVO...\n");
        return;
    }
    for (u32 index = 1; index < count; index++) {
        const struct fs_entry *entry = fs_find(arguments[index], working_directory);
        if (entry == 0) {
            u32 result = fs_create(arguments[index], working_directory, 0);
            if (result != FS_OK) {
                print_fs_error(result, arguments[index]);
            }
        } else if (entry->directory) {
            print_fs_error(FS_IS_DIRECTORY, arguments[index]);
        }
    }
}

static void command_remove(char **arguments, u32 count, u8 directory_only)
{
    u8 recursive = 0;
    u32 path_index = 1;

    if (!directory_only && count > 1 && strcmp(arguments[1], "-r") == 0) {
        recursive = 1;
        path_index = 2;
    }
    if (path_index >= count) {
        console_write(directory_only ? "Uso: rmdir DIRETORIO...\n" : "Uso: rm [-r] CAMINHO...\n");
        return;
    }

    for (u32 index = path_index; index < count; index++) {
        const struct fs_entry *entry = fs_find(arguments[index], working_directory);
        if (entry == 0) {
            print_fs_error(FS_NOT_FOUND, arguments[index]);
            continue;
        }
        if (directory_only && !entry->directory) {
            print_fs_error(FS_NOT_DIRECTORY, arguments[index]);
            continue;
        }
        if (!directory_only && entry->directory && !recursive) {
            print_fs_error(FS_IS_DIRECTORY, arguments[index]);
            continue;
        }
        u32 result = fs_remove(arguments[index], working_directory, recursive);
        if (result != FS_OK) {
            print_fs_error(result, arguments[index]);
        }
    }
}

static void command_copy(char **arguments, u32 count)
{
    if (count != 3) {
        console_write("Uso: cp ORIGEM DESTINO\n");
        return;
    }

    const struct fs_entry *source = fs_find(arguments[1], working_directory);
    if (source == 0) {
        print_fs_error(FS_NOT_FOUND, arguments[1]);
        return;
    }
    if (source->directory) {
        print_fs_error(FS_IS_DIRECTORY, arguments[1]);
        return;
    }

    char target[FS_PATH_SIZE];
    const struct fs_entry *destination = fs_find(arguments[2], working_directory);
    if (destination != 0 && destination->directory) {
        if (!join_path(target, destination->path, base_name(source->path))) {
            print_fs_error(FS_INVALID, arguments[2]);
            return;
        }
    } else if (fs_resolve(arguments[2], working_directory, target) != FS_OK) {
        print_fs_error(FS_INVALID, arguments[2]);
        return;
    }

    u32 result = fs_write(target, "/", source->content, 0);
    if (result != FS_OK) {
        print_fs_error(result, target);
    }
}

static void command_wc(char **arguments, u32 count)
{
    if (count < 2) {
        console_write("Uso: wc ARQUIVO...\n");
        return;
    }

    for (u32 argument = 1; argument < count; argument++) {
        const struct fs_entry *entry = fs_find(arguments[argument], working_directory);
        if (entry == 0 || entry->directory) {
            print_fs_error(entry == 0 ? FS_NOT_FOUND : FS_IS_DIRECTORY, arguments[argument]);
            continue;
        }

        u32 lines = 0;
        u32 words = 0;
        u8 in_word = 0;
        for (u32 index = 0; index < entry->size; index++) {
            char character = entry->content[index];
            if (character == '\n') {
                lines++;
            }
            if (is_space(character) || character == '\n' || character == '\r') {
                in_word = 0;
            } else if (!in_word) {
                words++;
                in_word = 1;
            }
        }

        console_write_u32(lines);
        console_putc(' ');
        console_write_u32(words);
        console_putc(' ');
        console_write_u32(entry->size);
        console_write("  ");
        console_write(base_name(entry->path));
        console_putc('\n');
    }
}

static void command_lines(char **arguments, u32 count, u8 last_lines)
{
    u32 first_file = 1;
    u32 requested = 10;
    if (count > 2 && strcmp(arguments[1], "-n") == 0) {
        requested = parse_number(arguments[2], 10);
        first_file = 3;
    }
    if (first_file >= count) {
        console_write(last_lines ? "Uso: tail [-n N] ARQUIVO\n" : "Uso: head [-n N] ARQUIVO\n");
        return;
    }

    for (u32 argument = first_file; argument < count; argument++) {
        const struct fs_entry *entry = fs_find(arguments[argument], working_directory);
        if (entry == 0 || entry->directory) {
            print_fs_error(entry == 0 ? FS_NOT_FOUND : FS_IS_DIRECTORY, arguments[argument]);
            continue;
        }

        u32 starts[FS_CONTENT_SIZE];
        u32 line_count = 0;
        if (entry->size > 0) {
            starts[line_count++] = 0;
        }
        for (u32 index = 0; index + 1 < entry->size; index++) {
            if (entry->content[index] == '\n') {
                starts[line_count++] = index + 1;
            }
        }

        u32 start_line = last_lines && line_count > requested ? line_count - requested : 0;
        u32 end_line = !last_lines && line_count > requested ? requested : line_count;
        for (u32 line = start_line; line < end_line; line++) {
            u32 index = starts[line];
            while (index < entry->size && entry->content[index] != '\n') {
                console_putc(entry->content[index++]);
            }
            console_putc('\n');
        }
    }
}

static u8 line_contains(const char *line, const char *pattern)
{
    if (*pattern == '\0') {
        return 1;
    }
    for (u32 start = 0; line[start] != '\0'; start++) {
        u32 index = 0;
        while (pattern[index] != '\0' && line[start + index] == pattern[index]) {
            index++;
        }
        if (pattern[index] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void command_grep(char **arguments, u32 count)
{
    if (count != 3) {
        console_write("Uso: grep PADRAO ARQUIVO\n");
        return;
    }
    const struct fs_entry *entry = fs_find(arguments[2], working_directory);
    if (entry == 0 || entry->directory) {
        print_fs_error(entry == 0 ? FS_NOT_FOUND : FS_IS_DIRECTORY, arguments[2]);
        return;
    }

    char line[FS_CONTENT_SIZE];
    u32 length = 0;
    for (u32 index = 0; index <= entry->size; index++) {
        char character = index == entry->size ? '\n' : entry->content[index];
        if (character == '\n') {
            line[length] = '\0';
            if (line_contains(line, arguments[1])) {
                console_write(line);
                console_putc('\n');
            }
            length = 0;
        } else if (length + 1 < sizeof(line)) {
            line[length++] = character;
        }
    }
}

static void remember_command(const char *line)
{
    if (*line == '\0') {
        return;
    }
    u32 index = 0;
    while (line[index] != '\0' && index + 1 < INPUT_SIZE) {
        history[history_next][index] = line[index];
        index++;
    }
    history[history_next][index] = '\0';
    history_next = (history_next + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

static void command_history(void)
{
    u32 start = history_count == HISTORY_SIZE ? history_next : 0;
    for (u32 index = 0; index < history_count; index++) {
        u32 slot = (start + index) % HISTORY_SIZE;
        console_write_u32(index + 1);
        console_write("  ");
        console_write(history[slot]);
        console_putc('\n');
    }
}

static void execute_command(char *line)
{
    char *arguments[ARGUMENT_COUNT];
    u32 count = tokenize(line, arguments, ARGUMENT_COUNT);
    if (count == 0) {
        return;
    }
    if (arguments[0][0] == '\0') {
        return;
    }

    if (strcmp(arguments[0], "help") == 0 || strcmp(arguments[0], "man") == 0) {
        console_write("Comandos: ls pwd cd cat echo touch mkdir rm rmdir cp mv\n");
        console_write("          wc head tail grep clear history uname whoami hostname\n");
        console_write("          uptime about help man reboot halt\n");
        console_write("Filesystem: somente em RAM; suporta caminhos relativos e absolutos.\n");
    } else if (strcmp(arguments[0], "ls") == 0) {
        command_ls(arguments, count);
    } else if (strcmp(arguments[0], "pwd") == 0) {
        console_write(working_directory);
        console_putc('\n');
    } else if (strcmp(arguments[0], "cd") == 0) {
        const char *path = count > 1 ? arguments[1] : "/home/carlinho";
        const struct fs_entry *entry = fs_find(path, working_directory);
        if (entry == 0) {
            print_fs_error(FS_NOT_FOUND, path);
        } else if (!entry->directory) {
            print_fs_error(FS_NOT_DIRECTORY, path);
        } else if (fs_resolve(path, working_directory, working_directory) != FS_OK) {
            print_fs_error(FS_INVALID, path);
        }
    } else if (strcmp(arguments[0], "cat") == 0) {
        command_cat(arguments, count);
    } else if (strcmp(arguments[0], "echo") == 0) {
        command_echo(arguments, count);
    } else if (strcmp(arguments[0], "touch") == 0) {
        command_touch(arguments, count);
    } else if (strcmp(arguments[0], "mkdir") == 0) {
        command_mkdir(arguments, count);
    } else if (strcmp(arguments[0], "rm") == 0) {
        command_remove(arguments, count, 0);
    } else if (strcmp(arguments[0], "rmdir") == 0) {
        command_remove(arguments, count, 1);
    } else if (strcmp(arguments[0], "cp") == 0) {
        command_copy(arguments, count);
    } else if (strcmp(arguments[0], "mv") == 0) {
        if (count != 3) {
            console_write("Uso: mv ORIGEM DESTINO\n");
        } else {
            u32 result = fs_move(arguments[1], arguments[2], working_directory);
            if (result != FS_OK) {
                print_fs_error(result, arguments[1]);
            }
        }
    } else if (strcmp(arguments[0], "wc") == 0) {
        command_wc(arguments, count);
    } else if (strcmp(arguments[0], "head") == 0) {
        command_lines(arguments, count, 0);
    } else if (strcmp(arguments[0], "tail") == 0) {
        command_lines(arguments, count, 1);
    } else if (strcmp(arguments[0], "grep") == 0) {
        command_grep(arguments, count);
    } else if (strcmp(arguments[0], "clear") == 0) {
        console_clear();
    } else if (strcmp(arguments[0], "history") == 0) {
        command_history();
    } else if (strcmp(arguments[0], "uname") == 0) {
        console_write(count > 1 && strcmp(arguments[1], "-a") == 0
            ? "Carlinho-OS carlinho 0.1.0 x86 i386\n"
            : "Carlinho-OS\n");
    } else if (strcmp(arguments[0], "whoami") == 0) {
        console_write("carlinho\n");
    } else if (strcmp(arguments[0], "hostname") == 0) {
        const struct fs_entry *entry = fs_find("/etc/hostname", "/");
        if (entry != 0) {
            console_write(entry->content);
        }
    } else if (strcmp(arguments[0], "uptime") == 0) {
        console_write("Ligado ha ");
        console_write_u32(pit_uptime_seconds());
        console_write(" segundos\n");
    } else if (strcmp(arguments[0], "about") == 0) {
        console_write("Carlinho-OS 0.1.0 - sistema x86 experimental.\n");
    } else if (strcmp(arguments[0], "reboot") == 0) {
        interrupts_disable();
        while ((inb(0x64) & 0x02) != 0) {
        }
        outb(0x64, 0xFE);
        for (;;) {
            __asm__ volatile ("hlt");
        }
    } else if (strcmp(arguments[0], "halt") == 0) {
        interrupts_disable();
        console_write("Sistema desligado.\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    } else if (arguments[0] == pipe_operator) {
        console_write("Pipes ainda nao estao disponiveis.\n");
    } else {
        console_write("Comando nao encontrado: ");
        console_write(arguments[0]);
        console_write(". Digite help.\n");
    }
}

static void prompt(void)
{
    console_set_color(10, 0);
    console_write("carlinho:");
    console_write(working_directory);
    console_set_color(15, 0);
    console_write("$ ");
}

void shell_init(void)
{
    input_length = 0;
    console_write("Ambiente grafico Carlinho-OS. Digite help.\n");
    prompt();
}

void shell_handle_key(char character)
{
    if (character == '\n') {
        console_putc('\n');
        input[input_length] = '\0';
        remember_command(input);
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
