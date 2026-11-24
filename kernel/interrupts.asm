BITS 32

section .text
extern interrupt_handler
global interrupt_stub_table

%macro ISR_NO_ERROR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp interrupt_common
%endmacro

%macro ISR_ERROR 1
global isr%1
isr%1:
    push dword %1
    jmp interrupt_common
%endmacro

%assign vector 0
%rep 48
    %if vector = 8 || vector = 10 || vector = 11 || vector = 12 || vector = 13 || vector = 14 || vector = 17 || vector = 21 || vector = 29 || vector = 30
        ISR_ERROR vector
    %else
        ISR_NO_ERROR vector
    %endif
    %assign vector vector + 1
%endrep

interrupt_common:
    pusha
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    cld
    push esp
    call interrupt_handler
    add esp, 4
    popa
    add esp, 8
    iretd

align 4
interrupt_stub_table:
%assign vector 0
%rep 48
    dd isr%+vector
    %assign vector vector + 1
%endrep
