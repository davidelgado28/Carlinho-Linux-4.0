#include "string.h"

u32 strlen(const char *text)
{
    u32 length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

i32 strcmp(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }

    return (i32)(u8)*left - (i32)(u8)*right;
}
