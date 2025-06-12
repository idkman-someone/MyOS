#include "task.h"
#include "kernel.h"
#include "mm.h"
#include "timer.h"
#include <string.h>

// Global task management variables
static task_t* task_list_head_ptr = NULL;
static task_t* current_task = NULL;
static task_id_t next_task_id = 1;
static scheduler_stats_t scheduler_stats = {0};
static spinlock_t task_list_lock;
static spinlock_t scheduler_lock;
static uint32_t scheduler_quantum = 50; // Default 50ms time slice

// Task ready queues (one per priority level)
static task_t* ready_queues[5] = {NULL}; // IDLE, LOW, NORMAL, HIGH, CRITICAL

// Idle task stack and TCB
static uint8_t idle_task_stack[TASK_STACK_SIZE] __attribute__((aligned(16)));
static task_t idle_task_tcb;

// Forward declarations
static void schedule_next_task(void);
static void add_to_ready_queue(task_t* task);
static void remove_from_ready_queue(task_t* task);
static int init_task_cpu_state(task_t* task, void (*entry_point)(void));
static task_t* find_next_ready_task(void);

// Initialize task management system
int task_init(void) {
    // Initialize locks
    spinlock_init(&task_list_lock);
    spinlock_init(&scheduler_lock);
    
    // Initialize FPU
    fpu_init();
    
    // Initialize scheduler stats
    memset(&scheduler_stats, 0, sizeof(scheduler_stats_t));
    
    // Create idle task
    memset(&idle_task_tcb, 0, sizeof(task_t));
    idle_task_tcb.id = 0;  // Special ID for idle task
    strncpy(idle_task_tcb.name, "idle", TASK_NAME_MAX - 1);
    idle_task_tcb.state = TASK_READY;
    idle_task_tcb.priority = PRIORITY_IDLE;
    idle_task_tcb.flags = TASK_FLAG_KERNEL | TASK_FLAG_SYSTEM;
    idle_task_tcb.stack_base = idle_task_stack;
    idle_task_tcb.stack_size = TASK_STACK_SIZE;
    idle_task_tcb.time_slice = scheduler_quantum;
    idle_task_tcb.time_slice_remaining = scheduler_quantum;
    idle_task_tcb.creation_time = timer_get_ticks();
    
    // Initialize idle task CPU state
    if (init_task_cpu_state(&idle_task_tcb, idle_task) != 0) {
        return -1;
    }
    
    // Add idle task to task list and ready queue
    task_list_add(&idle_task_tcb);
    add_to_ready_queue(&idle_task_tcb);
    
    // Set as current task initially
    current_task = &idle_task_tcb;
    
    return 0;
}

// Create a new task
task_id_t task_create(const char* name, void (*entry_point)(void), 
                      task_priority_t priority, uint32_t flags) {
    if (!name || !entry_point) {
        return 0; // Invalid parameters
    }
    
    // Allocate memory for task control block
    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) {
        return 0; // Out of memory
    }
    
    // Initialize task structure
    memset(task, 0, sizeof(task_t));
    
    spinlock_acquire(&task_list_lock);
    task->id = next_task_id++;
    spinlock_release(&task_list_lock);
    
    strncpy(task->name, name, TASK_NAME_MAX - 1);
    task->name[TASK_NAME_MAX - 1] = '\0';
    task->state = TASK_READY;
    task->priority = priority;
    task->flags = flags;
    task->parent = current_task;
    task->time_slice = scheduler_quantum;
    task->time_slice_remaining = scheduler_quantum;
    task->creation_time = timer_get_ticks();
    task->exit_code = 0;
    
    // Allocate stack
    if (task_alloc_stack(task, TASK_STACK_SIZE) != 0) {
        kfree(task);
        return 0;
    }
    
    // Initialize CPU state
    if (init_task_cpu_state(task, entry_point) != 0) {
        task_free_stack(task);
        kfree(task);
        return 0;
    }
    
    // Setup memory management
    if (task_setup_memory(task) != 0) {
        task_free_stack(task);
        kfree(task);
        return 0;
    }
    
    // Add to task list and ready queue
    task_list_add(task);
    scheduler_add_task(task);
    
    // Update statistics
    spinlock_acquire(&scheduler_lock);
    scheduler_stats.total_tasks++;
    scheduler_stats.ready_tasks++;
    spinlock_release(&scheduler_lock);
    
    return task->id;
}

// Initialize CPU state for a new task
static int init_task_cpu_state(task_t* task, void (*entry_point)(void)) {
    cpu_state_t* state = &task->cpu_state;
    
    // Clear CPU state
    memset(state, 0, sizeof(cpu_state_t));
    
    // Set up stack pointer (stack grows downward)
    state->rsp = (uint64_t)task->stack_base + task->stack_size - 16;
    state->rbp = state->rsp;
    
    // Set entry point
    state->rip = (uint64_t)entry_point;
    
    // Set up segment registers
    if (task->flags & TASK_FLAG_USER) {
        state->cs = 0x1B;   // User code segment
        state->ds = 0x23;   // User data segment
        state->es = 0x23;
        state->fs = 0x23;
        state->gs = 0x23;
        state->ss = 0x23;   // User stack segment
    } else {
        state->cs = 0x08;   // Kernel code segment
        state->ds = 0x10;   // Kernel data segment
        state->es = 0x10;
        state->fs = 0x10;
        state->gs = 0x10;
        state->ss = 0x10;   // Kernel stack segment
    }
    
    // Set flags (enable interrupts)
    state->rflags = 0x200;
    
    // Set page directory (inherit from parent or use kernel)
    if (task->parent && task->parent->page_directory) {
        state->cr3 = task->parent->page_directory;
    } else {
        state->cr3 = get_kernel_page_directory();
    }
    task->page_directory = state->cr3;
    
    return 0;
}

// Destroy a task
int task_destroy(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    if (!task || task->id == 0) { // Can't destroy idle task
        return -1;
    }
    
    // Remove from scheduler
    scheduler_remove_task(task);
    
    // Clean up memory
    task_cleanup_memory(task);
    task_free_stack(task);
    
    // Free FPU state if allocated
    if (task->fpu_state) {
        kfree(task->fpu_state);
    }
    
    // Remove from task list
    task_list_remove(task);
    
    // Update statistics
    spinlock_acquire(&scheduler_lock);
    scheduler_stats.total_tasks--;
    spinlock_release(&scheduler_lock);
    
    // Free task control block
    kfree(task);
    
    return 0;
}

// Get current task
task_t* task_get_current(void) {
    return current_task;
}

// Get task by ID
task_t* task_get_by_id(task_id_t task_id) {
    spinlock_acquire(&task_list_lock);
    
    task_t* task = task_list_head_ptr;
    while (task) {
        if (task->id == task_id) {
            spinlock_release(&task_list_lock);
            return task;
        }
        task = task->next;
    }
    
    spinlock_release(&task_list_lock);
    return NULL;
}

// Get current task ID
task_id_t task_get_current_id(void) {
    return current_task ? current_task->id : 0;
}

// Suspend task
int task_suspend(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    if (!task || task->id == 0) {
        return -1;
    }
    
    spinlock_acquire(&scheduler_lock);
    if (task->state == TASK_RUNNING || task->state == TASK_READY) {
        task->state = TASK_SUSPENDED;
        remove_from_ready_queue(task);
        scheduler_stats.ready_tasks--;
    }
    spinlock_release(&scheduler_lock);
    
    // If suspending current task, schedule another
    if (task == current_task) {
        task_yield();
    }
    
    return 0;
}

// Resume task
int task_resume(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    if (!task || task->state != TASK_SUSPENDED) {
        return -1;
    }
    
    spinlock_acquire(&scheduler_lock);
    task->state = TASK_READY;
    add_to_ready_queue(task);
    scheduler_stats.ready_tasks++;
    spinlock_release(&scheduler_lock);
    
    return 0;
}

// Exit current task
void task_exit(int exit_code) {
    if (!current_task || current_task->id == 0) {
        return; // Can't exit idle task
    }
    
    current_task->exit_code = exit_code;
    current_task->state = TASK_TERMINATED;
    
    spinlock_acquire(&scheduler_lock);
    remove_from_ready_queue(current_task);
    scheduler_stats.ready_tasks--;
    scheduler_stats.running_tasks--;
    spinlock_release(&scheduler_lock);
    
    // Schedule next task
    schedule_next_task();
}

// Yield CPU to next task
void task_yield(void) {
    if (!current_task) return;
    
    // Reset time slice
    current_task->time_slice_remaining = current_task->time_slice;
    
    schedule_next_task();
}

// Sleep for specified milliseconds
void task_sleep(uint32_t milliseconds) {
    if (!current_task || current_task->id == 0) {
        return;
    }
    
    current_task->sleep_until = timer_get_ticks() + (milliseconds * timer_get_frequency() / 1000);
    current_task->state = TASK_BLOCKED;
    
    spinlock_acquire(&scheduler_lock);
    remove_from_ready_queue(current_task);
    scheduler_stats.ready_tasks--;
    scheduler_stats.blocked_tasks++;
    spinlock_release(&scheduler_lock);
    
    schedule_next_task();
}

// Add task to appropriate ready queue
static void add_to_ready_queue(task_t* task) {
    if (!task || task->priority > PRIORITY_CRITICAL) {
        return;
    }
    
    spinlock_acquire(&scheduler_lock);
    
    // Add to front of priority queue (FIFO within priority)
    task->next = ready_queues[task->priority];
    if (ready_queues[task->priority]) {
        ready_queues[task->priority]->prev = task;
    }
    ready_queues[task->priority] = task;
    task->prev = NULL;
    
    spinlock_release(&scheduler_lock);
}

// Remove task from ready queue
static void remove_from_ready_queue(task_t* task) {
    if (!task || task->priority > PRIORITY_CRITICAL) {
        return;
    }
    
    spinlock_acquire(&scheduler_lock);
    
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        ready_queues[task->priority] = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
    
    spinlock_release(&scheduler_lock);
}

// Find next ready task (priority-based scheduling)
static task_t* find_next_ready_task(void) {
    // Check priorities from highest to lowest
    for (int priority = PRIORITY_CRITICAL; priority >= PRIORITY_IDLE; priority--) {
        if (ready_queues[priority]) {
            return ready_queues[priority];
        }
    }
    return NULL;
}

// Schedule next task
static void schedule_next_task(void) {
    task_t* next_task = find_next_ready_task();
    
    if (!next_task) {
        // No tasks ready, something is wrong
        kernel_panic("No ready tasks in scheduler");
        return;
    }
    
    // If same task, just return
    if (next_task == current_task && current_task->state == TASK_RUNNING) {
        return;
    }
    
    // Update current task state
    task_t* old_task = current_task;
    if (old_task && old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    
    // Set new current task
    current_task = next_task;
    current_task->state = TASK_RUNNING;
    current_task->last_run = timer_get_ticks();
    current_task->context_switch_count++;
    
    // Update statistics
    scheduler_stats.context_switches++;
    
    // Perform context switch
    if (old_task && old_task != current_task) {
        context_switch(&old_task->cpu_state, &current_task->cpu_state);
    } else if (!old_task) {
        // First task switch
        context_switch(NULL, &current_task->cpu_state);
    }
}

// Scheduler initialization
void scheduler_init(void) {
    // Already initialized in task_init()
}

// Main scheduler function (called by timer interrupt)
void scheduler_tick(void) {
    if (!current_task) return;
    
    // Update CPU time
    current_task->cpu_time++;
    scheduler_stats.total_cpu_time++;
    
    // Check for sleeping tasks that should wake up
    uint64_t current_ticks = timer_get_ticks();
    
    spinlock_acquire(&task_list_lock);
    task_t* task = task_list_head_ptr;
    while (task) {
        if (task->state == TASK_BLOCKED && task->sleep_until > 0) {
            if (current_ticks >= task->sleep_until) {
                task->sleep_until = 0;
                task->state = TASK_READY;
                add_to_ready_queue(task);
                
                spinlock_acquire(&scheduler_lock);
                scheduler_stats.blocked_tasks--;
                scheduler_stats.ready_tasks++;
                spinlock_release(&scheduler_lock);
            }
        }
        task = task->next;
    }
    spinlock_release(&task_list_lock);
    
    // Decrement time slice
    if (current_task->time_slice_remaining > 0) {
        current_task->time_slice_remaining--;
    }
    
    // Preempt if time slice expired or higher priority task is ready
    if (current_task->time_slice_remaining == 0 || 
        (ready_queues[PRIORITY_CRITICAL] && current_task->priority < PRIORITY_CRITICAL) ||
        (ready_queues[PRIORITY_HIGH] && current_task->priority < PRIORITY_HIGH)) {
        
        // Reset time slice
        current_task->time_slice_remaining = current_task->time_slice;
        
        // Schedule next task
        schedule_next_task();
    }
}

// Idle task implementation
void idle_task(void) {
    while (1) {
        // Halt processor until next interrupt
        asm volatile("hlt");
        
        // Update idle time statistics
        scheduler_stats.idle_time++;
    }
}

// Task list management
task_t* task_list_head(void) {
    return task_list_head_ptr;
}

void task_list_add(task_t* task) {
    if (!task) return;
    
    spinlock_acquire(&task_list_lock);
    
    task->next = task_list_head_ptr;
    task->prev = NULL;
    
    if (task_list_head_ptr) {
        task_list_head_ptr->prev = task;
    }
    
    task_list_head_ptr = task;
    
    spinlock_release(&task_list_lock);
}

void task_list_remove(task_t* task) {
    if (!task) return;
    
    spinlock_acquire(&task_list_lock);
    
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        task_list_head_ptr = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
    
    spinlock_release(&task_list_lock);
}

// Scheduler management functions
void scheduler_add_task(task_t* task) {
    if (!task) return;
    add_to_ready_queue(task);
}

void scheduler_remove_task(task_t* task) {
    if (!task) return;
    remove_from_ready_queue(task);
}

// Memory management for tasks
int task_alloc_stack(task_t* task, size_t size) {
    if (!task || size == 0) return -1;
    
    task->stack_base = kmalloc(size);
    if (!task->stack_base) {
        return -1;
    }
    
    task->stack_size = size;
    return 0;
}

void task_free_stack(task_t* task) {
    if (task && task->stack_base) {
        kfree(task->stack_base);
        task->stack_base = NULL;
        task->stack_size = 0;
    }
}

int task_setup_memory(task_t* task) {
    if (!task) return -1;
    
    // Basic memory setup - inherit from parent or use kernel space
    if (task->flags & TASK_FLAG_USER) {
        // User tasks need their own page directory
        // This would involve setting up virtual memory mapping
        // For now, just use kernel page directory
        task->page_directory = get_kernel_page_directory();
    } else {
        task->page_directory = get_kernel_page_directory();
    }
    
    return 0;
}

void task_cleanup_memory(task_t* task) {
    if (!task) return;
    
    // Clean up task-specific memory mappings
    // This would involve freeing page tables, etc.
    // For now, just clear the page directory reference
    task->page_directory = 0;
}

// FPU management
int task_enable_fpu(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    if (!task) return -1;
    
    if (!task->fpu_state) {
        task->fpu_state = (fpu_state_t*)kmalloc(sizeof(fpu_state_t));
        if (!task->fpu_state) {
            return -1;
        }
        
        // Initialize FPU state
        memset(task->fpu_state, 0, sizeof(fpu_state_t));
    }
    
    task->flags |= TASK_FLAG_FPU;
    return 0;
}

void task_disable_fpu(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    if (!task) return;
    
    if (task->fpu_state) {
        kfree(task->fpu_state);
        task->fpu_state = NULL;
    }
    
    task->flags &= ~TASK_FLAG_FPU;
}

// Statistics and debugging
void task_get_scheduler_stats(scheduler_stats_t* stats) {
    if (!stats) return;
    
    spinlock_acquire(&scheduler_lock);
    *stats = scheduler_stats;
    spinlock_release(&scheduler_lock);
}

void task_print_list(void) {
    spinlock_acquire(&task_list_lock);
    
    kprintf("Task List:\n");
    kprintf("ID\tName\t\tState\tPriority\tCPU Time\n");
    
    task_t* task = task_list_head_ptr;
    while (task) {
        const char* state_str;
        switch (task->state) {
            case TASK_RUNNING: state_str = "RUNNING"; break;
            case TASK_READY: state_str = "READY"; break;
            case TASK_BLOCKED: state_str = "BLOCKED"; break;
            case TASK_SUSPENDED: state_str = "SUSPENDED"; break;
            case TASK_TERMINATED: state_str = "TERMINATED"; break;
            default: state_str = "UNKNOWN"; break;
        }
        
        kprintf("%u\t%-12s\t%s\t%u\t\t%llu\n", 
                task->id, task->name, state_str, task->priority, task->cpu_time);
        
        task = task->next;
    }
    
    spinlock_release(&task_list_lock);
}

// Spinlock implementation
void spinlock_init(spinlock_t* lock) {
    if (!lock) return;
    
    lock->locked = 0;
    lock->owner = 0;
    lock->count = 0;
    lock->recursion_count = 0;
}

void spinlock_acquire(spinlock_t* lock) {
    if (!lock) return;
    
    // Simple test-and-set spinlock
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        // Spin until lock is available
        while (lock->locked) {
            asm volatile("pause"); // CPU hint for spinloop
        }
    }
    
    lock->owner = task_get_current_id();
    lock->count++;
}

void spinlock_release(spinlock_t* lock) {
    if (!lock || lock->owner != task_get_current_id()) {
        return;
    }
    
    lock->owner = 0;
    __sync_lock_release(&lock->locked);
}

bool spinlock_try_acquire(spinlock_t* lock) {
    if (!lock) return false;
    
    if (__sync_lock_test_and_set(&lock->locked, 1) == 0) {
        lock->owner = task_get_current_id();
        lock->count++;
        return true;
    }
    
    return false;
}

bool spinlock_is_held(spinlock_t* lock) {
    return lock ? (lock->locked != 0) : false;
}

// Kernel task creation helper
int kernel_task_create(const char* name, void (*entry_point)(void)) {
    task_id_t id = task_create(name, entry_point, PRIORITY_NORMAL, 
                               TASK_FLAG_KERNEL | TASK_FLAG_SYSTEM);
    return (id != 0) ? 0 : -1;
}

// Priority management
int task_set_priority(task_id_t task_id, task_priority_t priority) {
    task_t* task = task_get_by_id(task_id);
    if (!task || priority > PRIORITY_CRITICAL) {
        return -1;
    }
    
    // Remove from current priority queue
    if (task->state == TASK_READY) {
        remove_from_ready_queue(task);
    }
    
    // Update priority
    task->priority = priority;
    
    // Add back to appropriate queue
    if (task->state == TASK_READY) {
        add_to_ready_queue(task);
    }
    
    return 0;
}

task_priority_t task_get_priority(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    return task ? task->priority : PRIORITY_IDLE;
}

// Scheduler quantum management
int scheduler_set_quantum(uint32_t quantum_ms) {
    if (quantum_ms == 0 || quantum_ms > 1000) {
        return -1; // Invalid quantum
    }
    
    scheduler_quantum = quantum_ms;
    return 0;
}

// Basic task information functions
const char* task_get_name(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    return task ? task->name : NULL;
}

task_state_t task_get_state(task_id_t task_id) {
    task_t* task = task_get_by_id(task_id);
    return task ? task->state : TASK_TERMINATED;
}