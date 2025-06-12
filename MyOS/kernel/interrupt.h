#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

// Register structure for interrupt handlers
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

// Function prototypes
int init_interrupts(void);
void fault_handler(struct registers* r);
void irq_handler(struct registers* r);
void timer_handler(void);
void keyboard_handler(void);

#endif // INTERRUPT_H