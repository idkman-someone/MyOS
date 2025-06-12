; boot/gdt.asm - Global Descriptor Table setup
section .text
bits 32

; GDT structure
align 8
gdt_start:
    dq 0x0000000000000000 ; Null descriptor

; 32-bit code segment
gdt_code_32:
    dw 0xFFFF       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0x9A         ; Access byte (present, ring 0, code, executable, readable)
    db 0xCF         ; Flags (granularity, 32-bit) + Limit (16-19)
    db 0x00         ; Base (24-31)

; 32-bit data segment
gdt_data_32:
    dw 0xFFFF       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0x92         ; Access byte (present, ring 0, data, writable)
    db 0xCF         ; Flags (granularity, 32-bit) + Limit (16-19)
    db 0x00         ; Base (24-31)

; 64-bit code segment
gdt_code_64:
    dw 0x0000       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0x9A         ; Access byte (present, ring 0, code, executable, readable)
    db 0xAF         ; Flags (long mode) + Limit (16-19)
    db 0x00         ; Base (24-31)

; 64-bit data segment
gdt_data_64:
    dw 0x0000       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0x92         ; Access byte (present, ring 0, data, writable)
    db 0xCF         ; Flags (long mode) + Limit (16-19)
    db 0x00         ; Base (24-31)

gdt_end:

; GDT descriptor
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

; Load GDT and update segments
global load_gdt
load_gdt:
    lgdt [gdt_descriptor]
    
    ; Reload code segment
    push 0x08        ; Code segment selector
    lea rax, [rel .reload_cs]
    push rax
    retfq
    
.reload_cs:
    ; Reload data segments
    mov ax, 0x10     ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret