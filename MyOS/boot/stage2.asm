; stage2.asm - Enhanced long mode setup + kernel loader
BITS 16
ORG 0x8000

%define KERNEL_ENTRY     0x100000
%define KERNEL_LBA       5       ; LBA of kernel start
%define MAX_KERNEL_SECT  128     ; Max kernel size (64KB)

; Stage2 signature
dw 0xB007

start_stage2:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print initialization message
    mov si, init_msg
    call print_string_16

    ; Check CPU features
    call check_cpu
    jc .cpu_error

    ; Get memory map
    call detect_memory

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enable A20 line
    call enable_a20
    jc .a20_error

    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to 32-bit code segment
    jmp 0x08:protected_mode

.cpu_error:
    mov si, cpu_err_msg
    call print_string_16
    jmp $

.a20_error:
    mov si, a20_err_msg
    call print_string_16
    jmp $

[BITS 32]
protected_mode:
    ; Set up protected mode segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00

    ; Setup paging
    call setup_paging

    ; Check if long mode is available
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)       ; LME bit
    wrmsr

    ; Enable PAE and paging
    mov eax, cr4
    or eax, (1 << 5)       ; PAE
    mov cr4, eax

    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr0
    or eax, (1 << 31) | 1  ; PG + PE
    mov cr0, eax

    ; Long jump to 64-bit mode
    jmp 0x28:long_mode_start

.no_long_mode:
    mov esi, no_long_msg
    call print_string_32
    jmp $

[BITS 64]
long_mode_start:
    mov ax, 0x30
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x7C00

    ; Load kernel
    call load_kernel
    jc .load_error

    ; Jump to kernel
    jmp kernel_entry

.load_error:
    mov rsi, load_err_msg
    call print_string_64
    jmp $

; 16-bit functions
[BITS 16]
print_string_16:
    pusha
    mov ah, 0x0E
.repeat:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .repeat
.done:
    popa
    ret

; Check CPU features (returns CF=1 if incompatible)
check_cpu:
    ; Check for CPUID
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_cpuid

    ; Check for required features
    mov eax, 1
    cpuid
    test edx, (1 << 16)    ; PSE
    jz .missing_feature
    test edx, (1 << 4)     ; TSC
    jz .missing_feature
    clc
    ret

.no_cpuid:
.missing_feature:
    stc
    ret

; Enable A20 line (returns CF=1 on error)
enable_a20:
    ; Try BIOS method first
    mov ax, 0x2401
    int 0x15
    jnc .success

    ; Try keyboard controller method
    call .wait_kbd
    mov al, 0xAD
    out 0x64, al

    call .wait_kbd
    mov al, 0xD0
    out 0x64, al

    call .wait_kbd_in
    in al, 0x60
    push eax

    call .wait_kbd
    mov al, 0xD1
    out 0x64, al

    call .wait_kbd
    pop eax
    or al, 2
    out 0x60, al

    call .wait_kbd
    mov al, 0xAE
    out 0x64, al

    ; Test A20
    call test_a20
    jc .error
.success:
    clc
    ret
.error:
    stc
    ret

.wait_kbd:
    in al, 0x64
    test al, 2
    jnz .wait_kbd
    ret

.wait_kbd_in:
    in al, 0x64
    test al, 1
    jz .wait_kbd_in
    ret

test_a20:
    push es
    push di
    push cx

    xor ax, ax
    mov es, ax
    mov di, 0x0500

    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x0510

    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF

    cmp byte [es:di], 0xFF
    je .a20_off

    clc
    jmp .done
.a20_off:
    stc
.done:
    pop cx
    pop di
    pop es
    ret

; Detect memory (using INT 0x15, EAX=0xE820)
detect_memory:
    mov di, memory_map
    xor ebx, ebx
    mov edx, 0x534D4150
.loop:
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .done
    test ebx, ebx
    jz .done
    add di, 24
    inc byte [memory_entries]
    jmp .loop
.done:
    ret

; 32-bit functions
[BITS 32]
print_string_32:
    pusha
    mov edx, 0xB8000
.loop:
    lodsb
    test al, al
    jz .done
    mov [edx], al
    add edx, 2
    jmp .loop
.done:
    popa
    ret

; Setup paging (identity maps first 4GB)
setup_paging:
    ; Clear page tables
    mov edi, pml4_table
    mov ecx, 0x3000 / 4
    xor eax, eax
    rep stosd

    ; Build PML4
    mov eax, pdpt_table
    or eax, 0x03      ; Present + writable
    mov [pml4_table], eax
    mov [pml4_table + 0xFF8], eax  ; Recursive mapping

    ; Build PDPT (1GB pages)
    mov eax, 0x83     ; Present + writable + huge
    mov edi, pdpt_table
    mov ecx, 4        ; Map 4GB
.loop_pdpt:
    mov [edi], eax
    add eax, 0x40000000
    add edi, 8
    loop .loop_pdpt
    ret

; 64-bit functions
[BITS 64]
print_string_64:
    push rdi
    push rsi
    push rax
    mov rdi, 0xB8000
.loop:
    lodsb
    test al, al
    jz .done
    stosb
    inc rdi
    jmp .loop
.done:
    pop rax
    pop rsi
    pop rdi
    ret

; Load kernel from disk
load_kernel:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Set up disk address packet
    mov rsi, kernel_dap
    mov word [rsi + 2], 1       ; Sectors per read
    mov word [rsi + 4], 0x1000  ; Offset (0x1000:0x0000 = 0x100000)
    mov word [rsi + 6], 0x0000  ; Segment
    mov dword [rsi + 8], KERNEL_LBA
    mov dword [rsi + 12], 0

    ; Read sectors
    mov rcx, MAX_KERNEL_SECT
    mov rdi, KERNEL_ENTRY
.read_loop:
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .error

    ; Move to next sector
    inc dword [rsi + 8]
    add rdi, 512
    loop .read_loop

    ; Check kernel signature
    cmp dword [KERNEL_ENTRY], 0x4E4C4E4B  ; 'KERN' magic
    jne .signature_error

    clc
    jmp .done

.error:
.signature_error:
    stc
.done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; Data structures
kernel_dap:
    db 0x10           ; Size of DAP
    db 0              ; Unused
    dw 0              ; Sector count (set at runtime)
    dw 0              ; Offset (set at runtime)
    dw 0              ; Segment (set at runtime)
    dq 0              ; LBA (set at runtime)

; Messages
init_msg: db "Stage2 loaded", 0x0D, 0x0A, 0
cpu_err_msg: db "CPU incompatible", 0x0D, 0x0A, 0
a20_err_msg: db "A20 enable failed", 0x0D, 0x0A, 0
no_long_msg: db "No long mode", 0x0D, 0x0A, 0
load_err_msg: db "Kernel load failed", 0x0D, 0x0A, 0

; Memory map
memory_entries: db 0
memory_map: times 24 * 32 db 0  ; Space for 32 entries

; GDT
align 8
gdt_start:
    dq 0x0000000000000000 ; Null
    dq 0x00CF9A000000FFFF ; 32-bit code
    dq 0x00CF92000000FFFF ; 32-bit data
    dq 0x00AF9A000000FFFF ; 64-bit code
    dq 0x00AF92000000FFFF ; 64-bit data

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

gdt_end:

; Page tables
align 4096
pml4_table: times 4096 db 0
pdpt_table: times 4096 db 0

; Boot drive (passed from stage1)
boot_drive: db 0

times 512*4 - ($ - $$) db 0 ; Padding