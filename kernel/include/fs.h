#ifndef CARLINHO_FS_H
#define CARLINHO_FS_H

#include "types.h"

#define FS_ENTRY_COUNT 64
#define FS_PATH_SIZE 96
#define FS_CONTENT_SIZE 512

#define FS_OK 0
#define FS_NOT_FOUND 1
#define FS_EXISTS 2
#define FS_NOT_DIRECTORY 3
#define FS_NOT_EMPTY 4
#define FS_NO_SPACE 5
#define FS_IS_DIRECTORY 6
#define FS_INVALID 7

struct fs_entry {
    char path[FS_PATH_SIZE];
    char content[FS_CONTENT_SIZE];
    u32 size;
    u8 directory;
    u8 used;
};

void fs_init(void);
u32 fs_resolve(const char *path, const char *cwd, char *output);
const struct fs_entry *fs_find(const char *path, const char *cwd);
u32 fs_list(const char *path, const char *cwd, const struct fs_entry **entries, u32 capacity, u32 *count);
u32 fs_create(const char *path, const char *cwd, u8 directory);
u32 fs_write(const char *path, const char *cwd, const char *content, u8 append);
u32 fs_remove(const char *path, const char *cwd, u8 recursive);
u32 fs_move(const char *source, const char *destination, const char *cwd);

#endif
