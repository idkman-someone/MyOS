// kernel/mm.c - Memory Management Implementation
#include "mm.h"
#include "kernel.h"
#include <stddef.h>
#include <string.h>

// Physical memory bitmap
static uint8_t* physical_bitmap = (uint8_t*)0x300000;  // Start at 3MB
static uint64_t total_memory = 0;
static uint64_t available_memory = 0;
static uint64_t bitmap_size = 0;

// Heap management
static heap_block_t* heap_start = NULL;
static heap_block_t* heap_end = NULL;
static bool heap_initialized = false;

// Page frame allocator
static uint64_t next_free_frame = 0;

// Initialize physical memory management
int init_paging() {
    // Get memory map from bootloader (simplified version)
    total_memory = 128 * 1024 * 1024;  // Assume 128MB for now
    available_memory = total_memory - 0x400000;  // Reserve first 4MB
    
    bitmap_size = (total_memory / PAGE_SIZE) / 8;
    
    // Clear bitmap
    memset(physical_bitmap, 0, bitmap_size);
    
    // Mark first 4MB as used (kernel space)
    for (uint64_t i = 0; i < 0x400000 / PAGE_SIZE; i++) {
        set_frame_used(i);
    }
    
    kprintf("Memory: %lluMB total, %lluMB available\n", 
            total_memory / (1024*1024), available_memory / (1024*1024));
    
    return 0;
}

// Initialize heap
int mm_init(uint64_t heap_start_addr, uint64_t heap_size) {
    heap_start = (heap_block_t*)heap_start_addr;
    heap_end = (heap_block_t*)(heap_start_addr + heap_size);
    
    // Initialize first block
    heap_start->size = heap_size - sizeof(heap_block_t);
    heap_start->free = true;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    
    heap_initialized = true;
    kprintf("Heap initialized: 0x%llx - 0x%llx (%lluKB)\n", 
            heap_start_addr, heap_start_addr + heap_size, heap_size / 1024);
    
    return 0;
}

// Allocate physical frame
uint64_t alloc_frame() {
    for (uint64_t i = next_free_frame; i < total_memory / PAGE_SIZE; i++) {
        if (!is_frame_used(i)) {
            set_frame_used(i);
            next_free_frame = i + 1;
            return i * PAGE_SIZE;
        }
    }
    
    // Search from beginning
    for (uint64_t i = 0; i < next_free_frame; i++) {
        if (!is_frame_used(i)) {
            set_frame_used(i);
            next_free_frame = i + 1;
            return i * PAGE_SIZE;
        }
    }
    
    return 0;  // Out of memory
}

// Free physical frame
void free_frame(uint64_t frame_addr) {
    uint64_t frame = frame_addr / PAGE_SIZE;
    if (frame < total_memory / PAGE_SIZE) {
        clear_frame_used(frame);
        if (frame < next_free_frame) {
            next_free_frame = frame;
        }
    }
}

// Check if frame is used
bool is_frame_used(uint64_t frame) {
    uint64_t byte = frame / 8;
    uint8_t bit = frame % 8;
    return physical_bitmap[byte] & (1 << bit);
}

// Set frame as used
void set_frame_used(uint64_t frame) {
    uint64_t byte = frame / 8;
    uint8_t bit = frame % 8;
    physical_bitmap[byte] |= (1 << bit);
}

// Clear frame used flag
void clear_frame_used(uint64_t frame) {
    uint64_t byte = frame / 8;
    uint8_t bit = frame % 8;
    physical_bitmap[byte] &= ~(1 << bit);
}

// Kernel malloc implementation
void* kmalloc(size_t size) {
    if (!heap_initialized || size == 0) {
        return NULL;
    }
    
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    // Find free block
    heap_block_t* current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            // Split block if necessary
            if (current->size > size + sizeof(heap_block_t) + 8) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->free = true;
                new_block->next = current->next;
                new_block->prev = current;
                
                if (current->next) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = size;
            }
            
            current->free = false;
            return (uint8_t*)current + sizeof(heap_block_t);
        }
        current = current->next;
    }
    
    return NULL;  // Out of memory
}

// Kernel free implementation
void kfree(void* ptr) {
    if (!ptr || !heap_initialized) {
        return;
    }
    
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    
    // Validate block
    if ((uint8_t*)block < (uint8_t*)heap_start || (uint8_t*)block >= (uint8_t*)heap_end) {
        return;
    }
    
    block->free = true;
    
    // Coalesce with next block
    if (block->next && block->next->free) {
        block->size += block->next->size + sizeof(heap_block_t);
        if (block->next->next) {
            block->next->next->prev = block;
        }
        block->next = block->next->next;
    }
    
    // Coalesce with previous block
    if (block->prev && block->prev->free) {
        block->prev->size += block->size + sizeof(heap_block_t);
        if (block->next) {
            block->next->prev = block->prev;
        }
        block->prev->next = block->next;
    }
}

// Kernel calloc implementation
void* kcalloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = kmalloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

// Kernel realloc implementation
void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    if (block->size >= new_size) {
        return ptr;  // Current block is large enough
    }
    
    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size < new_size ? block->size : new_size);
        kfree(ptr);
    }
    
    return new_ptr;
}

// Get memory statistics
void get_memory_stats(memory_stats_t* stats) {
    if (!stats) return;
    
    stats->total_memory = total_memory;
    stats->available_memory = available_memory;
    stats->used_memory = 0;
    stats->free_heap = 0;
    stats->used_heap = 0;
    
    // Calculate used physical memory
    for (uint64_t i = 0; i < total_memory / PAGE_SIZE; i++) {
        if (is_frame_used(i)) {
            stats->used_memory += PAGE_SIZE;
        }
    }
    
    // Calculate heap statistics
    if (heap_initialized) {
        heap_block_t* current = heap_start;
        while (current) {
            if (current->free) {
                stats->free_heap += current->size;
            } else {
                stats->used_heap += current->size;
            }
            current = current->next;
        }
    }
}