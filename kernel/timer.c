#include "timer.h"
#include "port_io.h"
#include "interrupt.h"
#include "console.h"

#define PIT_FREQUENCY 1193182
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

static uint64_t ticks = 0;

static void timer_callback(struct registers* regs) {
    ticks++;
    // Add scheduler tick here if needed
}

void timer_init(uint32_t frequency) {
    // Register timer interrupt handler
    register_interrupt_handler(IRQ0, timer_callback);

    // Calculate divisor
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Configure PIT
    outb(PIT_COMMAND, 0x36); // Channel 0, lobyte/hibyte, mode 3
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    kprintf("Timer initialized at %dHz\n", frequency);
}

uint64_t timer_get_ticks() {
    return ticks;
}

void timer_sleep(uint64_t ms) {
    uint64_t target = ticks + ms;
    while (ticks < target) {
        asm volatile ("hlt");
    }
}