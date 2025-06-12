#include "interrupt.h"
#include "console.h"
#include "port_io.h"

// IDT Entry structure
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

// IDT Pointer structure
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// IDT table (256 entries)
static struct idt_entry idt[256];
static struct idt_ptr idtp;

// Exception names
static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

// Forward declarations of assembly interrupt handlers
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// IRQ handlers
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// Install an IDT entry
static void idt_install_gate(int num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].zero = 0;
}

// Initialize the IDT
static void idt_install(void) {
    // Set up IDT pointer
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint64_t)&idt;

    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt_install_gate(i, 0, 0, 0);
    }

    // Install exception handlers (ISRs 0-31)
    idt_install_gate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt_install_gate(1, (uint64_t)isr1, 0x08, 0x8E);
    idt_install_gate(2, (uint64_t)isr2, 0x08, 0x8E);
    idt_install_gate(3, (uint64_t)isr3, 0x08, 0x8E);
    idt_install_gate(4, (uint64_t)isr4, 0x08, 0x8E);
    idt_install_gate(5, (uint64_t)isr5, 0x08, 0x8E);
    idt_install_gate(6, (uint64_t)isr6, 0x08, 0x8E);
    idt_install_gate(7, (uint64_t)isr7, 0x08, 0x8E);
    idt_install_gate(8, (uint64_t)isr8, 0x08, 0x8E);
    idt_install_gate(9, (uint64_t)isr9, 0x08, 0x8E);
    idt_install_gate(10, (uint64_t)isr10, 0x08, 0x8E);
    idt_install_gate(11, (uint64_t)isr11, 0x08, 0x8E);
    idt_install_gate(12, (uint64_t)isr12, 0x08, 0x8E);
    idt_install_gate(13, (uint64_t)isr13, 0x08, 0x8E);
    idt_install_gate(14, (uint64_t)isr14, 0x08, 0x8E);
    idt_install_gate(15, (uint64_t)isr15, 0x08, 0x8E);
    idt_install_gate(16, (uint64_t)isr16, 0x08, 0x8E);
    idt_install_gate(17, (uint64_t)isr17, 0x08, 0x8E);
    idt_install_gate(18, (uint64_t)isr18, 0x08, 0x8E);
    idt_install_gate(19, (uint64_t)isr19, 0x08, 0x8E);
    idt_install_gate(20, (uint64_t)isr20, 0x08, 0x8E);
    idt_install_gate(21, (uint64_t)isr21, 0x08, 0x8E);
    idt_install_gate(22, (uint64_t)isr22, 0x08, 0x8E);
    idt_install_gate(23, (uint64_t)isr23, 0x08, 0x8E);
    idt_install_gate(24, (uint64_t)isr24, 0x08, 0x8E);
    idt_install_gate(25, (uint64_t)isr25, 0x08, 0x8E);
    idt_install_gate(26, (uint64_t)isr26, 0x08, 0x8E);
    idt_install_gate(27, (uint64_t)isr27, 0x08, 0x8E);
    idt_install_gate(28, (uint64_t)isr28, 0x08, 0x8E);
    idt_install_gate(29, (uint64_t)isr29, 0x08, 0x8E);
    idt_install_gate(30, (uint64_t)isr30, 0x08, 0x8E);
    idt_install_gate(31, (uint64_t)isr31, 0x08, 0x8E);

    // Install IRQ handlers (remapped to 32-47)
    idt_install_gate(32, (uint64_t)irq0, 0x08, 0x8E);
    idt_install_gate(33, (uint64_t)irq1, 0x08, 0x8E);
    idt_install_gate(34, (uint64_t)irq2, 0x08, 0x8E);
    idt_install_gate(35, (uint64_t)irq3, 0x08, 0x8E);
    idt_install_gate(36, (uint64_t)irq4, 0x08, 0x8E);
    idt_install_gate(37, (uint64_t)irq5, 0x08, 0x8E);
    idt_install_gate(38, (uint64_t)irq6, 0x08, 0x8E);
    idt_install_gate(39, (uint64_t)irq7, 0x08, 0x8E);
    idt_install_gate(40, (uint64_t)irq8, 0x08, 0x8E);
    idt_install_gate(41, (uint64_t)irq9, 0x08, 0x8E);
    idt_install_gate(42, (uint64_t)irq10, 0x08, 0x8E);
    idt_install_gate(43, (uint64_t)irq11, 0x08, 0x8E);
    idt_install_gate(44, (uint64_t)irq12, 0x08, 0x8E);
    idt_install_gate(45, (uint64_t)irq13, 0x08, 0x8E);
    idt_install_gate(46, (uint64_t)irq14, 0x08, 0x8E);
    idt_install_gate(47, (uint64_t)irq15, 0x08, 0x8E);

    // Load IDT
    asm volatile ("lidt %0" : : "m" (idtp));
}

// Remap PIC (Programmable Interrupt Controller)
static void pic_remap(void) {
    // Save masks
    uint8_t mask1 = inb(0x21);
    uint8_t mask2 = inb(0xA1);

    // Start initialization sequence
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    // Set vector offsets
    outb(0x21, 0x20);  // Master PIC: IRQ 0-7 -> interrupts 32-39
    outb(0xA1, 0x28);  // Slave PIC: IRQ 8-15 -> interrupts 40-47

    // Configure cascading
    outb(0x21, 0x04);  // Tell master PIC about slave at IRQ2
    outb(0xA1, 0x02);  // Tell slave PIC its cascade identity

    // Set mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Restore masks
    outb(0x21, mask1);
    outb(0xA1, mask2);
}

// Main interrupt initialization
int init_interrupts(void) {
    kprintf("- Setting up IDT...\n");
    idt_install();
    
    kprintf("- Remapping PIC...\n");
    pic_remap();
    
    kprintf("- Enabling interrupts...\n");
    asm volatile ("sti");
    
    return 0;
}

// Exception handler
void fault_handler(struct registers* r) {
    if (r->int_no < 32) {
        console_set_color(0x4F); // White on red
        kprintf("\nEXCEPTION: %s\n", exception_messages[r->int_no]);
        kprintf("Error code: %d\n", r->err_code);
        kprintf("EIP: 0x%x\n", r->rip);
        kprintf("CS: 0x%x\n", r->cs);
        kprintf("RFLAGS: 0x%x\n", r->rflags);
        kprintf("System halted\n");
        
        for (;;) {
            asm volatile ("cli; hlt");
        }
    }
}

// IRQ handler
void irq_handler(struct registers* r) {
    // Send EOI to PICs
    if (r->int_no >= 40) {
        outb(0xA0, 0x20); // Send EOI to slave PIC
    }
    outb(0x20, 0x20); // Send EOI to master PIC
    
    // Handle specific IRQs
    switch (r->int_no) {
        case 32: // Timer
            timer_handler();
            break;
        case 33: // Keyboard
            keyboard_handler();
            break;
        default:
            // Unhandled IRQ
            break;
    }
}

// Placeholder handlers (to be implemented)
void timer_handler(void) {
    // Timer tick - increment system ticks, handle scheduling, etc.
}

void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    // Process keyboard input
    // For now, just acknowledge
}