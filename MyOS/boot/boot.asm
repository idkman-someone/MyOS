; boot.asm - Enhanced 16-bit bootloader (stage 1)
BITS 16
ORG 0x7C00

%define STAGE2_SEGMENT   0x0000
%define STAGE2_OFFSET    0x8000
%define STAGE2_LBA       1       ; LBA of stage2 (sector 1)
%define STAGE2_SECTORS   4       ; Size of stage2 in sectors

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Save boot drive number (passed in DL by BIOS)
    mov [boot_drive], dl

    ; Print loading message
    mov si, loading_msg
    call print_string

    ; Check for LBA extensions
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc .use_chs          ; No LBA extensions, fallback to CHS
    cmp bx, 0xAA55
    jne .use_chs

    ; LBA read
    mov ah, 0x42
    mov si, dap
    mov dl, [boot_drive]
    int 0x13
    jnc .success         ; Jump if no error

.use_chs:
    ; CHS read (fallback)
    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0            ; Cylinder 0
    mov cl, STAGE2_LBA+1  ; Sector (LBA 1 = CHS sector 2)
    mov dh, 0            ; Head 0
    mov dl, [boot_drive]
    mov bx, STAGE2_OFFSET
    int 0x13
    jc disk_error        ; Jump on error

.success:
    ; Verify stage2 signature
    cmp word [STAGE2_OFFSET], 0xB007
    jne signature_error

    ; Jump to second stage
    jmp STAGE2_SEGMENT:STAGE2_OFFSET

; Data structures
dap:
    db 0x10           ; Size of DAP
    db 0              ; Unused
    dw STAGE2_SECTORS ; Sector count
    dw STAGE2_OFFSET  ; Offset
    dw STAGE2_SEGMENT ; Segment
    dq STAGE2_LBA     ; LBA

; Error handlers
disk_error:
    mov si, disk_msg
    call print_string
    jmp $

signature_error:
    mov si, signature_msg
    call print_string
    jmp $

; Print string function
print_string:
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

; Data
boot_drive: db 0
loading_msg: db "Loading stage2...", 0x0D, 0x0A, 0
disk_msg: db "Disk error!", 0x0D, 0x0A, 0
signature_msg: db "Invalid stage2!", 0x0D, 0x0A, 0

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55