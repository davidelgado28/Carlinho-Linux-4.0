#include "pit.h"
#include "ports.h"

static volatile u32 ticks;

void pit_init(u32 frequency)
{
    if (frequency == 0 || frequency > 1193182) {
        frequency = 100;
    }

    u32 divisor = 1193182 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (u8)(divisor & 0xFF));
    outb(0x40, (u8)((divisor >> 8) & 0xFF));
    ticks = 0;
}

u32 pit_uptime_seconds(void)
{
    return ticks / 100;
}

void pit_interrupt(void)
{
    ticks++;
}
