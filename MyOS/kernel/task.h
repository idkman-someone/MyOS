#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Task states
typedef enum {
    TASK_RUNNING = 0,
    TASK_READY,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_TERMINATED
} task_state_t;

// Task priority levels
typedef enum {
    PRIORITY_IDLE = 0,
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_HIGH = 3,
    PRIORITY_CRITICAL = 4
} task_priority_t;

// Task flags
#define TASK_FLAG_KERNEL    (1 << 0)   // Kernel task
#define TASK_FLAG_USER      (1 << 1)   // User task
#define TASK_FLAG_SYSTEM    (1 << 2)   // System task
#define TASK_FLAG_DAEMON    (1 << 3)   // Daemon task
#define TASK_FLAG_FPU       (1 << 4)   // Task uses FPU

// Maximum number of tasks
#define MAX_TASKS           256
#define TASK_STACK_SIZE     8192        // 8KB stack per task
#define TASK_NAME_MAX       32

// Task ID type
typedef uint32_t task_id_t;

// FPU/SSE state structure (512 bytes, 16-byte aligned)
typedef struct {
    uint8_t fxsave_region[512];
} __attribute__((aligned(16))) fpu_state_t;

// CPU register state structure (for context switching)
typedef struct {
    // General purpose registers
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    
    // Special registers
    uint64_t rip, rflags;
    
    // Segment registers
    uint64_t cs, ds, es, fs, gs, ss;
    
    // Control registers
    uint64_t cr3;           // Page directory base
    
    // Reserved for future use
    uint64_t reserved[3];
} __attribute__((packed)) cpu_state_t;

// Task control block
typedef struct task {
    task_id_t id;                       // Task ID
    char name[TASK_NAME_MAX];           // Task name
    task_state_t state;                 // Current state
    task_priority_t priority;           // Task priority
    uint32_t flags;                     // Task flags
    
    // CPU state
    cpu_state_t cpu_state;              // Saved CPU state
    fpu_state_t* fpu_state;             // FPU/SSE state (allocated if needed)
    void* stack_base;                   // Stack base address
    size_t stack_size;                  // Stack size
    
    // Memory management
    uint64_t page_directory;            // Page directory physical address
    void* heap_start;                   // Process heap start
    void* heap_end;                     // Process heap end
    
    // Timing information
    uint64_t creation_time;             // When task was created
    uint64_t cpu_time;                  // Total CPU time used
    uint64_t last_run;                  // Last time task ran
    uint32_t time_slice;                // Time slice in milliseconds
    uint32_t time_slice_remaining;      // Remaining time slice
    
    // Task relationships
    struct task* parent;                // Parent task
    struct task* next;                  // Next task in list
    struct task* prev;                  // Previous task in list
    
    // Synchronization
    uint64_t sleep_until;               // Sleep until this tick
    void* wait_object;                  // Object being waited on
    int wait_result;                    // Result of wait operation
    
    // File descriptors (simple implementation)
    void* file_descriptors[32];         // File descriptor table
    
    // Signal handling
    uint64_t signal_mask;               // Blocked signals
    void* signal_handlers[64];          // Signal handler table
    
    // Exit information
    int exit_code;                      // Exit code when terminated
    
    // Statistics
    uint64_t context_switch_count;      // Number of context switches
    uint64_t page_fault_count;          // Number of page faults
} task_t;

// Scheduler statistics
typedef struct {
    uint32_t total_tasks;               // Total number of tasks
    uint32_t running_tasks;             // Number of running tasks
    uint32_t ready_tasks;               // Number of ready tasks
    uint32_t blocked_tasks;             // Number of blocked tasks
    uint32_t context_switches;          // Total context switches
    uint64_t idle_time;                 // Total idle time
    uint64_t total_cpu_time;            // Total CPU time used
} scheduler_stats_t;

// Task management functions
int task_init(void);
task_id_t task_create(const char* name, void (*entry_point)(void), 
                      task_priority_t priority, uint32_t flags);
int task_destroy(task_id_t task_id);
int task_suspend(task_id_t task_id);
int task_resume(task_id_t task_id);
void task_exit(int exit_code);
void task_yield(void);

// Task information functions
task_t* task_get_current(void);
task_t* task_get_by_id(task_id_t task_id);
task_id_t task_get_current_id(void);
const char* task_get_name(task_id_t task_id);
task_state_t task_get_state(task_id_t task_id);

// Priority and scheduling functions
int task_set_priority(task_id_t task_id, task_priority_t priority);
task_priority_t task_get_priority(task_id_t task_id);
void task_sleep(uint32_t milliseconds);
int task_wait_for(void* object, uint32_t timeout_ms);
int task_notify(void* object);
int task_broadcast(void* object);

// Scheduler functions
void scheduler_init(void);
void scheduler_run(void);
void scheduler_tick(void);
task_t* scheduler_get_next_task(void);
void scheduler_add_task(task_t* task);
void scheduler_remove_task(task_t* task);
int scheduler_set_quantum(uint32_t quantum_ms);

// Context switching (implemented in assembly)
void context_switch(cpu_state_t* old_state, cpu_state_t* new_state);
void fast_context_switch(cpu_state_t* old_state, cpu_state_t* new_state);
void task_switch_to(task_t* task);
void save_context(cpu_state_t* state);
void restore_context(cpu_state_t* state);
void interrupt_save_context(void);
void interrupt_restore_context(void);

// FPU context management
void fpu_save_context(fpu_state_t* state);
void fpu_restore_context(fpu_state_t* state);
void fpu_init(void);
int task_enable_fpu(task_id_t task_id);
void task_disable_fpu(task_id_t task_id);

// Statistics and debugging
void task_get_scheduler_stats(scheduler_stats_t* stats);
void task_print_list(void);
void task_print_info(task_id_t task_id);
void task_dump_context(task_id_t task_id);

// Kernel task functions
int kernel_task_create(const char* name, void (*entry_point)(void));
void idle_task(void);

// Lock-free task list operations
task_t* task_list_head(void);
void task_list_add(task_t* task);
void task_list_remove(task_t* task);

// Memory management for tasks
int task_alloc_stack(task_t* task, size_t size);
void task_free_stack(task_t* task);
int task_setup_memory(task_t* task);
void task_cleanup_memory(task_t* task);

// Signal handling (basic implementation)
typedef void (*signal_handler_t)(int signal);
int task_set_signal_handler(task_id_t task_id, int signal, signal_handler_t handler);
int task_send_signal(task_id_t task_id, int signal);
int task_block_signal(task_id_t task_id, int signal);
int task_unblock_signal(task_id_t task_id, int signal);

// Task synchronization primitives
typedef struct {
    volatile uint32_t locked;
    task_id_t owner;
    uint32_t count;
    uint32_t recursion_count;
} spinlock_t;

typedef struct {
    volatile uint32_t count;
    task_t* waiting_tasks;
    spinlock_t lock;
} semaphore_t;

typedef struct {
    volatile uint32_t locked;
    task_t* waiting_tasks;
    task_id_t owner;
    uint32_t recursion_count;
    spinlock_t lock;
} mutex_t;

// Spinlock operations
void spinlock_init(spinlock_t* lock);
void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);
bool spinlock_try_acquire(spinlock_t* lock);
bool spinlock_is_held(spinlock_t* lock);

// Semaphore operations
void semaphore_init(semaphore_t* sem, uint32_t initial_count);
void semaphore_wait(semaphore_t* sem);
void semaphore_post(semaphore_t* sem);
bool semaphore_try_wait(semaphore_t* sem);
uint32_t semaphore_get_count(semaphore_t* sem);

// Mutex operations
void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);
bool mutex_try_lock(mutex_t* mutex);
bool mutex_is_held(mutex_t* mutex);

// Common task entry points
void user_task_wrapper(void (*entry_point)(void));
void kernel_task_wrapper(void (*entry_point)(void));

// Performance monitoring
typedef struct {
    uint64_t user_time;
    uint64_t kernel_time;
    uint64_t context_switches;
    uint64_t page_faults;
    uint64_t system_calls;
} task_perf_t;

int task_get_performance(task_id_t task_id, task_perf_t* perf);
void task_reset_performance(task_id_t task_id);

// CPU affinity (for SMP systems)
typedef uint64_t cpu_set_t;

int task_set_affinity(task_id_t task_id, cpu_set_t cpuset);
int task_get_affinity(task_id_t task_id, cpu_set_t* cpuset);

// Task groups/processes
typedef uint32_t process_id_t;

process_id_t task_get_process_id(task_id_t task_id);
int task_set_process_group(task_id_t task_id, process_id_t pgid);

#endif // TASK_H