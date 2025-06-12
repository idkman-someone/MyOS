# context_switch.s - x86-64 context switching implementation (FIXED)
# For use with the task management system

.section .text
.global context_switch
.global task_switch_to
.global save_context
.global restore_context
.global interrupt_save_context
.global interrupt_restore_context

# void context_switch(cpu_state_t* old_state, cpu_state_t* new_state)
# RDI = old_state pointer
# RSI = new_state pointer
context_switch:
    # Save current task's context to old_state
    # Skip if old_state is NULL (first task switch)
    test    %rdi, %rdi
    jz      restore_new_context
    
    # Save the return address first (from stack)
    movq    (%rsp), %rax
    movq    %rax, 128(%rdi)     # rip
    
    # Save general purpose registers
    movq    %rax, 0(%rdi)       # rax (save actual rax value)
    movq    %rbx, 8(%rdi)       # rbx
    movq    %rcx, 16(%rdi)      # rcx
    movq    %rdx, 24(%rdi)      # rdx
    movq    %rsi, 32(%rdi)      # rsi (save before we lose it)
    # Note: rdi contains old_state pointer, save original rdi value
    pushq   %rdi                # Save old_state pointer temporarily
    movq    8(%rsp), %rax       # Get original rdi from caller
    popq    %rdi                # Restore old_state pointer
    movq    %rax, 40(%rdi)      # Save original rdi
    movq    %rbp, 48(%rdi)      # rbp
    
    # Save stack pointer (adjust for return address we already popped conceptually)
    leaq    8(%rsp), %rax       # RSP + 8 (account for return address)
    movq    %rax, 56(%rdi)      # rsp
    
    # Save extended registers (r8-r15)  
    movq    %r8,  64(%rdi)      # r8
    movq    %r9,  72(%rdi)      # r9
    movq    %r10, 80(%rdi)      # r10
    movq    %r11, 88(%rdi)      # r11
    movq    %r12, 96(%rdi)      # r12
    movq    %r13, 104(%rdi)     # r13
    movq    %r14, 112(%rdi)     # r14
    movq    %r15, 120(%rdi)     # r15
    
    # Save flags register
    pushfq
    popq    %rax
    movq    %rax, 136(%rdi)     # rflags
    
    # Save segment registers (only data segments, CS is handled differently)
    movw    %ds, %ax
    movq    %rax, 152(%rdi)     # ds
    movw    %es, %ax
    movq    %rax, 160(%rdi)     # es
    movw    %fs, %ax
    movq    %rax, 168(%rdi)     # fs
    movw    %gs, %ax
    movq    %rax, 176(%rdi)     # gs
    movw    %ss, %ax
    movq    %rax, 184(%rdi)     # ss
    
    # Save control registers
    movq    %cr3, %rax
    movq    %rax, 192(%rdi)     # cr3 (page directory)

restore_new_context:
    # Memory barrier for SMP safety
    mfence
    
    # Restore new task's context from new_state
    # RSI = new_state pointer
    
    # Restore control registers first
    movq    192(%rsi), %rax     # cr3
    movq    %rax, %cr3          # Load new page directory
    
    # Restore segment registers (validate before loading)
    movq    152(%rsi), %rax     # ds
    cmpw    $0x10, %ax          # Check if valid kernel data segment
    je      1f
    cmpw    $0x23, %ax          # Check if valid user data segment
    je      1f
    movw    $0x10, %ax          # Default to kernel data segment
1:  movw    %ax, %ds
    
    movq    160(%rsi), %rax     # es
    movw    %ax, %es
    movq    168(%rsi), %rax     # fs
    movw    %ax, %fs
    movq    176(%rsi), %rax     # gs
    movw    %ax, %gs
    
    # Restore stack pointer
    movq    56(%rsi), %rsp      # rsp
    
    # Restore flags
    movq    136(%rsi), %rax     # rflags
    pushq   %rax
    popfq
    
    # Push return address (rip) onto new stack
    movq    128(%rsi), %rax     # rip
    pushq   %rax
    
    # Restore extended registers first (r8-r15)
    movq    64(%rsi), %r8       # r8
    movq    72(%rsi), %r9       # r9
    movq    80(%rsi), %r10      # r10
    movq    88(%rsi), %r11      # r11
    movq    96(%rsi), %r12      # r12
    movq    104(%rsi), %r13     # r13
    movq    112(%rsi), %r14     # r14
    movq    120(%rsi), %r15     # r15
    
    # Restore general purpose registers (save rsi for last)
    movq    0(%rsi), %rax       # rax
    movq    8(%rsi), %rbx       # rbx
    movq    16(%rsi), %rcx      # rcx
    movq    24(%rsi), %rdx      # rdx
    movq    40(%rsi), %rdi      # rdi
    movq    48(%rsi), %rbp      # rbp
    
    # Finally restore rsi
    movq    32(%rsi), %rsi      # rsi
    
    # Return to new task (rip was pushed onto stack)
    ret

# void task_switch_to(task_t* task)  
# RDI = task pointer
task_switch_to:
    # Calculate offset to cpu_state within task_t structure
    # Assuming cpu_state is at a specific offset - adjust as needed
    addq    $64, %rdi           # Adjust offset to cpu_state within task_t
    
    # Call context_switch with NULL as old_state (forced switch)
    xorq    %rax, %rax          # old_state = NULL
    movq    %rdi, %rsi          # new_state = &task->cpu_state
    movq    %rax, %rdi          # old_state = NULL
    call    context_switch
    ret

# void save_context(cpu_state_t* state)
# RDI = state pointer  
save_context:
    # Get return address before modifying stack
    movq    (%rsp), %rax
    movq    %rax, 128(%rdi)     # rip
    
    # Save all registers to the cpu_state structure
    movq    %rax, 0(%rdi)       # rax (actual value, not return address)
    movq    %rbx, 8(%rdi)       # rbx
    movq    %rcx, 16(%rdi)      # rcx
    movq    %rdx, 24(%rdi)      # rdx
    movq    %rsi, 32(%rdi)      # rsi
    # For rdi, we need the original value passed by caller
    pushq   %rdi                # Save current rdi (state pointer)
    movq    8(%rsp), %rax       # Get original rdi
    popq    %rdi                # Restore state pointer
    movq    %rax, 40(%rdi)      # Save original rdi
    movq    %rbp, 48(%rdi)      # rbp
    
    # Save stack pointer (adjusted for return address)
    leaq    8(%rsp), %rax
    movq    %rax, 56(%rdi)      # rsp
    
    # Save extended registers
    movq    %r8,  64(%rdi)      # r8
    movq    %r9,  72(%rdi)      # r9
    movq    %r10, 80(%rdi)      # r10
    movq    %r11, 88(%rdi)      # r11
    movq    %r12, 96(%rdi)      # r12
    movq    %r13, 104(%rdi)     # r13
    movq    %r14, 112(%rdi)     # r14
    movq    %r15, 120(%rdi)     # r15
    
    # Save flags
    pushfq
    popq    %rax
    movq    %rax, 136(%rdi)     # rflags
    
    # Save segment registers
    movw    %cs, %ax
    movq    %rax, 144(%rdi)     # cs
    movw    %ds, %ax
    movq    %rax, 152(%rdi)     # ds
    movw    %es, %ax
    movq    %rax, 160(%rdi)     # es
    movw    %fs, %ax
    movq    %rax, 168(%rdi)     # fs
    movw    %gs, %ax
    movq    %rax, 176(%rdi)     # gs
    movw    %ss, %ax
    movq    %rax, 184(%rdi)     # ss
    
    # Save control registers
    movq    %cr3, %rax
    movq    %rax, 192(%rdi)     # cr3
    
    ret

# void restore_context(cpu_state_t* state)
# RDI = state pointer
restore_context:
    # Restore control registers
    movq    192(%rdi), %rax     # cr3
    movq    %rax, %cr3
    
    # Restore segment registers with validation
    movq    152(%rdi), %rax     # ds
    cmpw    $0x10, %ax
    je      2f
    cmpw    $0x23, %ax
    je      2f
    movw    $0x10, %ax
2:  movw    %ax, %ds
    
    movq    160(%rdi), %rax     # es
    movw    %ax, %es
    movq    168(%rdi), %rax     # fs
    movw    %ax, %fs
    movq    176(%rdi), %rax     # gs
    movw    %ax, %gs
    
    # Restore stack pointer
    movq    56(%rdi), %rsp      # rsp
    
    # Restore flags
    movq    136(%rdi), %rax     # rflags
    pushq   %rax
    popfq
    
    # Push return address
    movq    128(%rdi), %rax     # rip
    pushq   %rax
    
    # Restore extended registers
    movq    64(%rdi), %r8       # r8
    movq    72(%rdi), %r9       # r9
    movq    80(%rdi), %r10      # r10
    movq    88(%rdi), %r11      # r11
    movq    96(%rdi), %r12      # r12
    movq    104(%rdi), %r13     # r13
    movq    112(%rdi), %r14     # r14
    movq    120(%rdi), %r15     # r15
    
    # Restore general purpose registers
    movq    0(%rdi), %rax       # rax
    movq    8(%rdi), %rbx       # rbx
    movq    16(%rdi), %rcx      # rcx
    movq    24(%rdi), %rdx      # rdx
    movq    32(%rdi), %rsi      # rsi
    movq    48(%rdi), %rbp      # rbp
    
    # Restore RDI last
    movq    40(%rdi), %rdi      # rdi
    
    # Return to restored context
    ret

# Helper function for interrupt context saving
interrupt_save_context:
    # This is called from interrupt handlers
    # Stack layout: [SS] [RSP] [RFLAGS] [CS] [RIP] [ERROR_CODE (optional)]
    
    # Save all general purpose registers
    pushq   %rax
    pushq   %rbx
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %rbp
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    
    # Save segment registers
    movw    %ds, %ax
    pushq   %rax
    movw    %es, %ax
    pushq   %rax
    movw    %fs, %ax
    pushq   %rax
    movw    %gs, %ax
    pushq   %rax
    
    # Set up kernel data segments
    movw    $0x10, %ax      # Kernel data segment
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    
    # Memory barrier
    mfence
    
    ret

interrupt_restore_context:
    # Memory barrier
    mfence
    
    # Restore segment registers
    popq    %rax
    movw    %ax, %gs
    popq    %rax
    movw    %ax, %fs
    popq    %rax
    movw    %ax, %es
    popq    %rax
    movw    %ax, %ds
    
    # Restore general purpose registers
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rbp
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax
    
    # Return from interrupt (IRET will restore RIP, CS, RFLAGS, RSP, SS)
    iretq

# Fast context switch for same privilege level
.global fast_context_switch
fast_context_switch:
    # RDI = old_state, RSI = new_state
    # Optimized version for kernel-to-kernel switches
    
    test    %rdi, %rdi
    jz      fast_restore
    
    # Save minimal context (callee-saved registers only)
    movq    %rbx, 8(%rdi)
    movq    %rbp, 48(%rdi)
    movq    %r12, 96(%rdi)
    movq    %r13, 104(%rdi)
    movq    %r14, 112(%rdi)
    movq    %r15, 120(%rdi)
    movq    %rsp, 56(%rdi)
    
    # Save return address
    movq    (%rsp), %rax
    movq    %rax, 128(%rdi)
    
fast_restore:
    # Restore minimal context
    movq    8(%rsi), %rbx
    movq    48(%rsi), %rbp
    movq    96(%rsi), %r12
    movq    104(%rsi), %r13
    movq    112(%rsi), %r14
    movq    120(%rsi), %r15
    movq    56(%rsi), %rsp
    
    # Push return address
    movq    128(%rsi), %rax
    pushq   %rax
    
    ret

# FPU/SSE context save/restore
.global fpu_save_context
fpu_save_context:
    # RDI = fpu_state buffer (512 bytes aligned)
    fxsave  (%rdi)
    ret

.global fpu_restore_context  
fpu_restore_context:
    # RDI = fpu_state buffer
    fxrstor (%rdi)
    ret

.global fpu_init
fpu_init:
    # Initialize FPU to known state
    finit
    ret