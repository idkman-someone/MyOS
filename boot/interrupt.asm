; MyOS Interrupt Handlers - boot/interrupt.asm
; Assembly stubs for interrupt handling

section .text
bits 64

; Macro for exception handlers (no error code)
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli
        push byte 0        ; Push dummy error code
        push byte %1       ; Push interrupt number
        jmp isr_common_stub
%endmacro

; Macro for exception handlers (with error code)
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli
        push byte %1       ; Push interrupt number
        jmp isr_common_stub
%endmacro

; Macro for IRQ handlers
%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push byte 0        ; Push dummy error code
        push byte %2       ; Push interrupt number
        jmp irq_common_stub
%endmacro

; Exception handlers (0-31)
ISR_NOERRCODE 0    ; Division by zero
ISR_NOERRCODE 1    ; Debug
ISR_NOERRCODE 2    ; Non-maskable interrupt
ISR_NOERRCODE 3    ; Breakpoint
ISR_NOERRCODE 4    ; Overflow
ISR_NOERRCODE 5    ; Bound range exceeded
ISR_NOERRCODE 6    ; Invalid opcode
ISR_NOERRCODE 7    ; Device not available
ISR_ERRCODE   8    ; Double fault
ISR_NOERRCODE 9    ; Coprocessor segment overrun
ISR_ERRCODE   10   ; Invalid TSS
ISR_ERRCODE   11   ; Segment not present
ISR_ERRCODE   12   ; Stack-segment fault
ISR_ERRCODE   13   ; General protection fault
ISR_ERRCODE   14   ; Page fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; x87 floating-point exception
ISR_ERRCODE   17   ; Alignment check
ISR_NOERRCODE 18   ; Machine check
ISR_NOERRCODE 19   ; SIMD floating-point exception
ISR_NOERRCODE 20   ; Virtualization exception
ISR_ERRCODE   21   ; Control protection exception
ISR_NOERRCODE 22   ; Reserved
ISR_NOERRCODE 23   ; Reserved
ISR_NOERRCODE 24   ; Reserved
ISR_NOERRCODE 25   ; Reserved
ISR_NOERRCODE 26   ; Reserved
ISR_NOERRCODE 27   ; Reserved
ISR_NOERRCODE 28   ; Reserved
ISR_NOERRCODE 29   ; Reserved
ISR_ERRCODE   30   ; Security exception
ISR_NOERRCODE 31   ; Reserved

; IRQ handlers (32-47)
IRQ 0,  32   ; Timer
IRQ 1,  33   ; Keyboard
IRQ 2,  34   ; Cascade
IRQ 3,  35   ; COM2
IRQ 4,  36   ; COM1
IRQ 5,  37   ; LPT2
IRQ 6,  38   ; Floppy
IRQ 7,  39   ; LPT1
IRQ 8,  40   ; CMOS real-time clock
IRQ 9,  41   ; Free for peripherals
IRQ 10, 42   ; Free for peripherals
IRQ 11, 43   ; Free for peripherals
IRQ 12, 44   ; PS2 mouse
IRQ 13, 45   ; FPU / Coprocessor / Inter-processor
IRQ 14, 46   ; Primary ATA
IRQ 15, 47   ; Secondary ATA

; Common ISR stub
extern fault_handler
isr_common_stub:
    ; Save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Set up data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call C handler
    mov rdi, rsp    ; Pass stack pointer as argument
    call fault_handler

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up error code and interrupt number
    add rsp, 16

    ; Return from interrupt
    iretq

; Common IRQ stub
extern irq_handler
irq_common_stub:
    ; Save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Set up data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call C handler
    mov rdi, rsp    ; Pass stack pointer as argument
    call irq_handler

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up error code and interrupt number
    add rsp, 16

    ; Return from interrupt
    iretq