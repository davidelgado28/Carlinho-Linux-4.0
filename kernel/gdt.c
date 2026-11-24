#include "gdt.h"
#include "types.h"

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} PACKED;

struct gdt_pointer {
    u16 limit;
    u32 base;
} PACKED;

static struct gdt_entry entries[3];
static struct gdt_pointer pointer;

extern void gdt_load(const struct gdt_pointer *descriptor);

static void set_gate(u32 index, u32 base, u32 limit, u8 access, u8 granularity)
{
    entries[index].base_low = (u16)(base & 0xFFFF);
    entries[index].base_middle = (u8)((base >> 16) & 0xFF);
    entries[index].base_high = (u8)((base >> 24) & 0xFF);
    entries[index].limit_low = (u16)(limit & 0xFFFF);
    entries[index].granularity = (u8)((limit >> 16) & 0x0F);
    entries[index].granularity |= (u8)(granularity & 0xF0);
    entries[index].access = access;
}

void gdt_init(void)
{
    pointer.limit = (u16)(sizeof(entries) - 1);
    pointer.base = (u32)entries;
    set_gate(0, 0, 0, 0, 0);
    set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_load(&pointer);
}
