#ifndef CARLINHO_PIT_H
#define CARLINHO_PIT_H

#include "types.h"

void pit_init(u32 frequency);
u32 pit_uptime_seconds(void);
void pit_interrupt(void);

#endif
