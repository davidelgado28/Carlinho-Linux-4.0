#include "fs.h"

static struct fs_entry entries[FS_ENTRY_COUNT];

static u32 text_length(const char *text)
{
    u32 length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static u8 copy_text(char *destination, const char *source, u32 capacity)
{
    u32 index = 0;
    while (source[index] != '\0' && index + 1 < capacity) {
        destination[index] = source[index];
        index++;
    }

    if (source[index] != '\0') {
        return 0;
    }

    destination[index] = '\0';
    return 1;
}

static u8 text_equal(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

static u8 text_starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

u32 fs_resolve(const char *path, const char *cwd, char *output)
{
    char combined[FS_PATH_SIZE * 2];
    u32 combined_length = 0;

    if (path == 0 || path[0] == '\0') {
        path = cwd;
    }

    if (path == 0 || path[0] == '\0') {
        path = "/";
    }

    if (path[0] != '/') {
        const char *base = cwd;
        if (base == 0 || base[0] == '\0') {
            base = "/";
        }

        while (base[combined_length] != '\0' && combined_length + 1 < sizeof(combined)) {
            combined[combined_length] = base[combined_length];
            combined_length++;
        }

        if (combined_length + 1 < sizeof(combined) && combined_length > 0 && combined[combined_length - 1] != '/') {
            combined[combined_length++] = '/';
        }
    }

    while (*path != '\0' && combined_length + 1 < sizeof(combined)) {
        combined[combined_length++] = *path++;
    }

    if (*path != '\0') {
        return FS_INVALID;
    }
    combined[combined_length] = '\0';

    u32 output_length = 1;
    output[0] = '/';
    output[1] = '\0';

    u32 index = 0;
    while (index < combined_length) {
        while (index < combined_length && combined[index] == '/') {
            index++;
        }

        char component[FS_PATH_SIZE];
        u32 component_length = 0;
        while (index < combined_length && combined[index] != '/') {
            if (component_length + 1 >= sizeof(component)) {
                return FS_INVALID;
            }
            component[component_length++] = combined[index++];
        }
        component[component_length] = '\0';

        if (component_length == 0 || text_equal(component, ".")) {
            continue;
        }

        if (text_equal(component, "..")) {
            if (output_length > 1) {
                while (output_length > 1 && output[output_length - 1] != '/') {
                    output_length--;
                }
                if (output_length > 1) {
                    output_length--;
                }
                output[output_length] = '\0';
            }
            continue;
        }

        u32 separator = output_length > 1 ? 1 : 0;
        if (output_length + separator + component_length >= FS_PATH_SIZE) {
            return FS_INVALID;
        }
        if (separator != 0) {
            output[output_length++] = '/';
        }
        for (u32 character = 0; character < component_length; character++) {
            output[output_length++] = component[character];
        }
        output[output_length] = '\0';
    }

    return FS_OK;
}

static struct fs_entry *entry_find(const char *path)
{
    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        if (entries[index].used && text_equal(entries[index].path, path)) {
            return &entries[index];
        }
    }
    return 0;
}

const struct fs_entry *fs_find(const char *path, const char *cwd)
{
    char normalized[FS_PATH_SIZE];
    if (fs_resolve(path, cwd, normalized) != FS_OK) {
        return 0;
    }
    return entry_find(normalized);
}

static u32 parent_path(const char *path, char *parent)
{
    u32 length = text_length(path);
    if (length == 0 || text_equal(path, "/")) {
        return FS_INVALID;
    }

    u32 separator = length;
    while (separator > 0 && path[separator - 1] != '/') {
        separator--;
    }

    if (separator == 0) {
        return FS_INVALID;
    }

    if (separator == 1) {
        parent[0] = '/';
        parent[1] = '\0';
        return FS_OK;
    }

    for (u32 index = 0; index < separator - 1; index++) {
        parent[index] = path[index];
    }
    parent[separator - 1] = '\0';
    return FS_OK;
}

static struct fs_entry *free_entry(void)
{
    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        if (!entries[index].used) {
            return &entries[index];
        }
    }
    return 0;
}

static u32 add_entry(const char *path, u8 directory, const char *content)
{
    struct fs_entry *entry = free_entry();
    if (entry == 0) {
        return FS_NO_SPACE;
    }

    if (!copy_text(entry->path, path, sizeof(entry->path))) {
        return FS_INVALID;
    }
    entry->content[0] = '\0';
    entry->size = 0;
    entry->directory = directory;
    entry->used = 1;

    if (!directory && content != 0) {
        if (!copy_text(entry->content, content, sizeof(entry->content))) {
            entry->used = 0;
            return FS_NO_SPACE;
        }
        entry->size = text_length(entry->content);
    }

    return FS_OK;
}

void fs_init(void)
{
    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        entries[index].used = 0;
    }

    add_entry("/", 1, 0);
    add_entry("/home", 1, 0);
    add_entry("/home/carlinho", 1, 0);
    add_entry("/etc", 1, 0);
    add_entry("/tmp", 1, 0);
    add_entry("/home/carlinho/README.txt", 0, "Bem-vindo ao Carlinho-OS.\nEste sistema ainda esta em desenvolvimento.\n");
    add_entry("/etc/hostname", 0, "carlinho\n");
    add_entry("/etc/os-release", 0, "NAME=Carlinho-OS\nVERSION=0.1.0\n");
}

u32 fs_list(const char *path, const char *cwd, const struct fs_entry **output, u32 capacity, u32 *count)
{
    char normalized[FS_PATH_SIZE];
    u32 result = fs_resolve(path, cwd, normalized);
    if (result != FS_OK) {
        return result;
    }

    struct fs_entry *directory = entry_find(normalized);
    if (directory == 0) {
        return FS_NOT_FOUND;
    }
    if (!directory->directory) {
        return FS_NOT_DIRECTORY;
    }

    *count = 0;
    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        struct fs_entry *entry = &entries[index];
        if (!entry->used || text_equal(entry->path, normalized)) {
            continue;
        }

        char parent[FS_PATH_SIZE];
        if (parent_path(entry->path, parent) == FS_OK && text_equal(parent, normalized)) {
            if (*count >= capacity) {
                return FS_NO_SPACE;
            }
            output[(*count)++] = entry;
        }
    }

    return FS_OK;
}

u32 fs_create(const char *path, const char *cwd, u8 directory)
{
    char normalized[FS_PATH_SIZE];
    u32 result = fs_resolve(path, cwd, normalized);
    if (result != FS_OK || text_equal(normalized, "/")) {
        return FS_INVALID;
    }
    if (entry_find(normalized) != 0) {
        return FS_EXISTS;
    }

    char parent[FS_PATH_SIZE];
    if (parent_path(normalized, parent) != FS_OK) {
        return FS_INVALID;
    }
    struct fs_entry *parent_entry = entry_find(parent);
    if (parent_entry == 0) {
        return FS_NOT_FOUND;
    }
    if (!parent_entry->directory) {
        return FS_NOT_DIRECTORY;
    }

    return add_entry(normalized, directory, 0);
}

u32 fs_write(const char *path, const char *cwd, const char *content, u8 append)
{
    char normalized[FS_PATH_SIZE];
    u32 result = fs_resolve(path, cwd, normalized);
    if (result != FS_OK || text_equal(normalized, "/")) {
        return FS_INVALID;
    }

    struct fs_entry *entry = entry_find(normalized);
    if (entry == 0) {
        result = fs_create(normalized, "/", 0);
        if (result != FS_OK) {
            return result;
        }
        entry = entry_find(normalized);
    }
    if (entry->directory) {
        return FS_IS_DIRECTORY;
    }

    u32 offset = append ? entry->size : 0;
    u32 length = text_length(content);
    if (offset + length + 1 > sizeof(entry->content)) {
        return FS_NO_SPACE;
    }
    for (u32 index = 0; index < length; index++) {
        entry->content[offset + index] = content[index];
    }
    entry->content[offset + length] = '\0';
    entry->size = offset + length;
    return FS_OK;
}

u32 fs_remove(const char *path, const char *cwd, u8 recursive)
{
    char normalized[FS_PATH_SIZE];
    u32 result = fs_resolve(path, cwd, normalized);
    if (result != FS_OK || text_equal(normalized, "/")) {
        return FS_INVALID;
    }

    struct fs_entry *entry = entry_find(normalized);
    if (entry == 0) {
        return FS_NOT_FOUND;
    }

    u32 prefix_length = text_length(normalized);
    if (entry->directory && !recursive) {
        for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
            if (entries[index].used &&
                text_starts_with(entries[index].path, normalized) &&
                entries[index].path[prefix_length] == '/') {
                return FS_NOT_EMPTY;
            }
        }
    }

    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        if (entries[index].used &&
            (text_equal(entries[index].path, normalized) ||
             (entry->directory && text_starts_with(entries[index].path, normalized) &&
              entries[index].path[prefix_length] == '/'))) {
            entries[index].used = 0;
        }
    }
    return FS_OK;
}

u32 fs_move(const char *source, const char *destination, const char *cwd)
{
    char source_path[FS_PATH_SIZE];
    char destination_path[FS_PATH_SIZE];
    if (fs_resolve(source, cwd, source_path) != FS_OK ||
        fs_resolve(destination, cwd, destination_path) != FS_OK ||
        text_equal(source_path, "/")) {
        return FS_INVALID;
    }

    struct fs_entry *source_entry = entry_find(source_path);
    if (source_entry == 0) {
        return FS_NOT_FOUND;
    }
    if (text_equal(source_path, destination_path)) {
        return FS_OK;
    }

    struct fs_entry *destination_entry = entry_find(destination_path);
    if (destination_entry != 0 && destination_entry->directory) {
        u32 target_length = text_length(destination_path);
        u32 source_length = text_length(source_path);
        u32 base = source_length;
        while (base > 0 && source_path[base - 1] != '/') {
            base--;
        }
        const char *name = source_path + base;
        if (target_length + 1 + text_length(name) >= sizeof(destination_path)) {
            return FS_INVALID;
        }
        destination_path[target_length++] = '/';
        for (u32 index = 0; name[index] != '\0'; index++) {
            destination_path[target_length++] = name[index];
        }
        destination_path[target_length] = '\0';
        if (text_equal(source_path, destination_path)) {
            return FS_OK;
        }
        destination_entry = entry_find(destination_path);
    }

    if (destination_entry != 0 && (destination_entry->directory || source_entry->directory)) {
        return FS_EXISTS;
    }

    u32 source_length = text_length(source_path);
    if (source_entry->directory &&
        text_starts_with(destination_path, source_path) &&
        destination_path[source_length] == '/') {
        return FS_INVALID;
    }

    char parent[FS_PATH_SIZE];
    if (parent_path(destination_path, parent) != FS_OK) {
        return FS_INVALID;
    }
    struct fs_entry *parent_entry = entry_find(parent);
    if (parent_entry == 0) {
        return FS_NOT_FOUND;
    }
    if (!parent_entry->directory) {
        return FS_NOT_DIRECTORY;
    }

    u32 destination_length = text_length(destination_path);
    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        struct fs_entry *entry = &entries[index];
        if (!entry->used ||
            !(text_equal(entry->path, source_path) ||
              (source_entry->directory && text_starts_with(entry->path, source_path) &&
               entry->path[source_length] == '/'))) {
            continue;
        }

        char new_path[FS_PATH_SIZE];
        u32 suffix = text_length(entry->path + source_length);
        if (destination_length + suffix >= sizeof(new_path)) {
            return FS_INVALID;
        }
        for (u32 character = 0; character < destination_length; character++) {
            new_path[character] = destination_path[character];
        }
        for (u32 character = 0; character <= suffix; character++) {
            new_path[destination_length + character] = entry->path[source_length + character];
        }

        struct fs_entry *collision = entry_find(new_path);
        if (collision != 0 && collision != entry && collision != destination_entry &&
            !(text_equal(collision->path, source_path) ||
              (source_entry->directory && text_starts_with(collision->path, source_path) &&
               collision->path[source_length] == '/'))) {
            return FS_EXISTS;
        }
    }

    if (destination_entry != 0) {
        destination_entry->used = 0;
    }

    for (u32 index = 0; index < FS_ENTRY_COUNT; index++) {
        struct fs_entry *entry = &entries[index];
        if (!entry->used ||
            !(text_equal(entry->path, source_path) ||
              (source_entry->directory && text_starts_with(entry->path, source_path) &&
               entry->path[source_length] == '/'))) {
            continue;
        }

        char new_path[FS_PATH_SIZE];
        u32 suffix = text_length(entry->path + source_length);
        for (u32 character = 0; character < destination_length; character++) {
            new_path[character] = destination_path[character];
        }
        for (u32 character = 0; character <= suffix; character++) {
            new_path[destination_length + character] = entry->path[source_length + character];
        }
        copy_text(entry->path, new_path, sizeof(entry->path));
    }

    return FS_OK;
}
