// mm.h - Memory Management Header
#ifndef MM_H
#define MM_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGNED(addr) (((addr) & (PAGE_SIZE - 1)) == 0)

// Memory allocation functions
void* kmalloc(size_t size);
void* kcalloc(size_t count, size_t size);
void kfree(void* ptr);

// Page management
int init_paging(void);
void* alloc_page(void);
void free_page(void* page);

// Memory manager initialization
int mm_init(uint64_t heap_start, size_t heap_size);

// Memory statistics
typedef struct {
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    size_t allocated_blocks;
} memory_stats_t;

void get_memory_stats(memory_stats_t* stats);

#endif // MM_H

// mm.c - Memory Management Implementation
#include "mm.h"
#include "kernel.h"
#include "console.h"
#include <stddef.h>

// Simple heap allocator
typedef struct block {
    size_t size;
    bool used;
    struct block* next;
    struct block* prev;
} block_t;

static block_t* heap_start = NULL;
static size_t heap_size = 0;
static size_t total_allocated = 0;

// Page directory and tables (identity mapped for now)
static uint64_t* pml4_table = (uint64_t*)0x1000;
static uint64_t* pdpt_table = (uint64_t*)0x2000;
static uint64_t* pd_table = (uint64_t*)0x3000;

int init_paging(void) {
    // Basic identity mapping for first 1GB
    // This should be expanded for a real OS
    
    // Clear page tables
    for (int i = 0; i < 512; i++) {
        pml4_table[i] = 0;
        pdpt_table[i] = 0;
        pd_table[i] = 0;
    }
    
    // Set up page directory pointer table
    pml4_table[0] = (uint64_t)pdpt_table | 0x03; // Present + writable
    
    // Set up page directory with 2MB pages
    pdpt_table[0] = (uint64_t)pd_table | 0x03;
    
    // Map first 1GB with 2MB pages
    for (int i = 0; i < 512; i++) {
        pd_table[i] = (i * 0x200000) | 0x83; // Present + writable + page size
    }
    
    return 0;
}

int mm_init(uint64_t start_addr, size_t size) {
    heap_start = (block_t*)start_addr;
    heap_size = size;
    
    // Initialize first block
    heap_start->size = size - sizeof(block_t);
    heap_start->used = false;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    
    kprintf("Memory manager initialized: %p - %p (%zu bytes)\n", 
           (void*)start_addr, (void*)(start_addr + size), size);
    
    return 0;
}

static block_t* find_free_block(size_t size) {
    block_t* current = heap_start;
    
    while (current) {
        if (!current->used && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static void split_block(block_t* block, size_t size) {
    if (block->size > size + sizeof(block_t)) {
        block_t* new_block = (block_t*)((char*)block + sizeof(block_t) + size);
        new_block->size = block->size - size - sizeof(block_t);
        new_block->used = false;
        new_block->next = block->next;
        new_block->prev = block;
        
        if (block->next) {
            block->next->prev = new_block;
        }
        
        block->next = new_block;
        block->size = size;
    }
}

static void merge_blocks(block_t* block) {
    // Merge with next block if free
    if (block->next && !block->next->used) {
        block->size += block->next->size + sizeof(block_t);
        if (block->next->next) {
            block->next->next->prev = block;
        }
        block->next = block->next->next;
    }
    
    // Merge with previous block if free
    if (block->prev && !block->prev->used) {
        block->prev->size += block->size + sizeof(block_t);
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    block_t* block = find_free_block(size);
    if (!block) {
        return NULL; // Out of memory
    }
    
    split_block(block, size);
    block->used = true;
    total_allocated += size;
    
    return (char*)block + sizeof(block_t);
}

void* kcalloc(size_t count, size_t size) {
    size_t total_size = count * size;
    void* ptr = kmalloc(total_size);
    
    if (ptr) {
        // Zero out the memory
        char* p = (char*)ptr;
        for (size_t i = 0; i < total_size; i++) {
            p[i] = 0;
        }
    }
    
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    block->used = false;
    total_allocated -= block->size;
    
    merge_blocks(block);
}

void* alloc_page(void) {
    return kmalloc(PAGE_SIZE);
}

void free_page(void* page) {
    kfree(page);
}

void get_memory_stats(memory_stats_t* stats) {
    stats->total_memory = heap_size;
    stats->used_memory = total_allocated;
    stats->free_memory = heap_size - total_allocated;
    
    // Count allocated blocks
    stats->allocated_blocks = 0;
    block_t* current = heap_start;
    while (current) {
        if (current->used) {
            stats->allocated_blocks++;
        }
        current = current->next;
    }
}