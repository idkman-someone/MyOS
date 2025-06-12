#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Debug levels
typedef enum {
    DEBUG_TRACE = 0,
    DEBUG_DEBUG = 1,
    DEBUG_INFO = 2,
    DEBUG_WARN = 3,
    DEBUG_ERROR = 4,
    DEBUG_FATAL = 5
} debug_level_t;

// Debug initialization and configuration
void debug_init(void);
void debug_set_level(debug_level_t level);
debug_level_t debug_get_level(void);
void debug_enable_serial(bool enable);
void debug_enable_console(bool enable);

// Basic debug output functions
void debug_printf(const char* format, ...);
void debug_print(debug_level_t level, const char* format, ...);

// Memory debugging utilities
void debug_dump_memory(const void* addr, size_t size);
void debug_stack_trace(void);

// Assert functionality
void debug_assert_fail(const char* expr, const char* file, int line, const char* func);

// Debug macros
#ifdef DEBUG
    #define ASSERT(expr) \
        do { \
            if (!(expr)) { \
                debug_assert_fail(#expr, __FILE__, __LINE__, __func__); \
            } \
        } while (0)
    
    #define DEBUG_TRACE(...) debug_print(DEBUG_TRACE, __VA_ARGS__)
    #define DEBUG_DEBUG(...) debug_print(DEBUG_DEBUG, __VA_ARGS__)
    #define DEBUG_INFO(...)  debug_print(DEBUG_INFO, __VA_ARGS__)
    #define DEBUG_WARN(...)  debug_print(DEBUG_WARN, __VA_ARGS__)
    #define DEBUG_ERROR(...) debug_print(DEBUG_ERROR, __VA_ARGS__)
    #define DEBUG_FATAL(...) debug_print(DEBUG_FATAL, __VA_ARGS__)
#else
    #define ASSERT(expr) ((void)0)
    #define DEBUG_TRACE(...) ((void)0)
    #define DEBUG_DEBUG(...) ((void)0)
    #define DEBUG_INFO(...)  ((void)0)
    #define DEBUG_WARN(...)  ((void)0)
    #define DEBUG_ERROR(...) ((void)0)
    #define DEBUG_FATAL(...) ((void)0)
#endif

// Always-on debug macros (even in release builds)
#define PANIC(...) \
    do { \
        debug_print(DEBUG_FATAL, __VA_ARGS__); \
        debug_stack_trace(); \
        asm volatile ("cli; hlt"); \
        __builtin_unreachable(); \
    } while (0)

#define BUG_ON(condition) \
    do { \
        if (unlikely(condition)) { \
            PANIC("BUG: %s at %s:%d in %s()\n", #condition, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

// Likely/unlikely hints for the compiler
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Memory barrier macros
#define barrier() asm volatile ("" ::: "memory")
#define mb()      asm volatile ("mfence" ::: "memory")
#define rmb()     asm volatile ("lfence" ::: "memory")
#define wmb()     asm volatile ("sfence" ::: "memory")

#endif // DEBUG_H