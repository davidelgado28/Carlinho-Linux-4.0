BITS 32

section .multiboot
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + (header_end - header_start))
    dw 3
    dw 0
    dd 12
    dd _start
    align 8
    dw 5
    dw 0
    dd 20
    dd 800
    dd 600
    dd 32
    align 8
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    xor ebp, ebp
    cld
    push ebx
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt

global gdt_load
gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]
    jmp 0x08:.reload

.reload:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
