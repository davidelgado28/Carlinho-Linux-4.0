#ifndef CARLINHO_IDT_H
#define CARLINHO_IDT_H

#include "types.h"

struct interrupt_frame {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 interrupt_number;
    u32 error_code;
    u32 eip;
    u32 cs;
    u32 eflags;
};

void idt_init(void);
void interrupt_handler(struct interrupt_frame *frame);

#endif
