#include "elf_loader.h"
#include "kernel.h"
#include "mm.h"
#include "filesystem.h"
#include "console.h"
#include <string.h>

// Forward declarations for internal functions
static ElfStatus elf_map_segments(const void* elf_data, const Elf64_Ehdr* header, ElfProcess* process);
static ElfStatus elf_setup_memory_layout(const void* elf_data, const Elf64_Ehdr* header, ElfProcess* process);
static bool elf_check_magic(const uint8_t* ident);

/**
 * Validate ELF header for compatibility with our kernel
 */
ElfStatus elf_validate_header(const Elf64_Ehdr* header) {
    if (header == NULL) {
        return ELF_ERROR_NULL_POINTER;
    }

    // Check ELF magic number
    if (!elf_check_magic(header->e_ident)) {
        return ELF_ERROR_INVALID_MAGIC;
    }

    // Check for 64-bit ELF
    if (header->e_ident[4] != ELF_CLASS_64) {
        return ELF_ERROR_INVALID_CLASS;
    }

    // Check endianness (little-endian)
    if (header->e_ident[5] != ELF_DATA_LSB) {
        return ELF_ERROR_INVALID_ENDIAN;
    }

    // Check ELF version
    if (header->e_ident[6] != ELF_VERSION || header->e_version != ELF_VERSION) {
        return ELF_ERROR_INVALID_VERSION;
    }

    // Check file type (executable or shared object)
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        return ELF_ERROR_INVALID_TYPE;
    }

    // Check machine type (x86-64)
    if (header->e_machine != EM_X86_64) {
        return ELF_ERROR_INVALID_MACHINE;
    }

    // Check for program headers
    if (header->e_phnum == 0 || header->e_phoff == 0) {
        return ELF_ERROR_NO_PROGRAM_HEADERS;
    }

    return ELF_OK;
}

/**
 * Load ELF executable from memory buffer
 */
ElfStatus elf_load_from_memory(const void* elf_data, size_t size, ElfProcess* process) {
    if (elf_data == NULL || process == NULL) {
        return ELF_ERROR_NULL_POINTER;
    }

    if (size < sizeof(Elf64_Ehdr)) {
        return ELF_ERROR_INVALID_MAGIC;
    }

    const Elf64_Ehdr* header = (const Elf64_Ehdr*)elf_data;
    ElfStatus status;

    // Validate the ELF header
    status = elf_validate_header(header);
    if (status != ELF_OK) {
        return status;
    }

    // Initialize process structure
    memset(process, 0, sizeof(ElfProcess));
    process->entry_point = header->e_entry;

    // Calculate memory requirements and setup layout
    status = elf_setup_memory_layout(elf_data, header, process);
    if (status != ELF_OK) {
        return status;
    }

    // Map all loadable segments
    status = elf_map_segments(elf_data, header, process);
    if (status != ELF_OK) {
        return status;
    }

    process->is_loaded = true;
    kprintf("ELF loaded successfully: entry=0x%lx, base=0x%lx, size=%lu bytes\n",
            process->entry_point, process->base_address, process->total_size);

    return ELF_OK;
}

/**
 * Load ELF executable from file system
 */
ElfStatus elf_load_from_file(const char* filename, ElfProcess* process) {
    if (filename == NULL || process == NULL) {
        return ELF_ERROR_NULL_POINTER;
    }

    // Open file (placeholder - needs actual filesystem implementation)
    // For now, return error as filesystem is not fully implemented
    kprintf("ELF file loading not yet supported (filesystem needed)\n");
    return ELF_ERROR_LOAD_FAILED;
}

/**
 * Load a single ELF segment into memory
 */
ElfStatus elf_load_segment(const void* elf_data, const Elf64_Phdr* phdr, uint64_t base_addr) {
    if (elf_data == NULL || phdr == NULL) {
        return ELF_ERROR_NULL_POINTER;
    }

    // Skip non-loadable segments
    if (phdr->p_type != PT_LOAD) {
        return ELF_OK;
    }

    uint64_t vaddr = base_addr + phdr->p_vaddr;
    const uint8_t* src = (const uint8_t*)elf_data + phdr->p_offset;

    // Allocate and map memory for the segment
    void* dest = mm_alloc_pages((phdr->p_memsz + 0xFFF) / 0x1000);
    if (dest == NULL) {
        return ELF_ERROR_MEMORY_ALLOCATION;
    }

    // Copy file data to memory
    if (phdr->p_filesz > 0) {
        memcpy(dest, src, phdr->p_filesz);
    }

    // Zero out BSS section if needed
    if (phdr->p_memsz > phdr->p_filesz) {
        memset((uint8_t*)dest + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
    }

    // Set appropriate memory permissions based on segment flags
    // Note: This would require page table manipulation in a full implementation
    if (phdr->p_flags & PF_X) {
        // Mark as executable
    }
    if (phdr->p_flags & PF_W) {
        // Mark as writable
    }
    if (phdr->p_flags & PF_R) {
        // Mark as readable
    }

    return ELF_OK;
}

/**
 * Calculate total memory size required for ELF loading
 */
uint64_t elf_calculate_load_size(const void* elf_data) {
    if (elf_data == NULL) {
        return 0;
    }

    const Elf64_Ehdr* header = (const Elf64_Ehdr*)elf_data;
    if (elf_validate_header(header) != ELF_OK) {
        return 0;
    }

    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((const uint8_t*)elf_data + header->e_phoff);
    uint64_t min_addr = UINT64_MAX;
    uint64_t max_addr = 0;

    // Find the memory range needed
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t seg_start = phdr[i].p_vaddr;
            uint64_t seg_end = seg_start + phdr[i].p_memsz;

            if (seg_start < min_addr) {
                min_addr = seg_start;
            }
            if (seg_end > max_addr) {
                max_addr = seg_end;
            }
        }
    }

    return (min_addr == UINT64_MAX) ? 0 : (max_addr - min_addr);
}

/**
 * Get human-readable error string
 */
const char* elf_get_error_string(ElfStatus status) {
    switch (status) {
        case ELF_OK:
            return "Success";
        case ELF_ERROR_INVALID_MAGIC:
            return "Invalid ELF magic number";
        case ELF_ERROR_INVALID_CLASS:
            return "Not a 64-bit ELF file";
        case ELF_ERROR_INVALID_ENDIAN:
            return "Invalid endianness";
        case ELF_ERROR_INVALID_VERSION:
            return "Unsupported ELF version";
        case ELF_ERROR_INVALID_TYPE:
            return "Invalid file type";
        case ELF_ERROR_INVALID_MACHINE:
            return "Unsupported machine type";
        case ELF_ERROR_NO_PROGRAM_HEADERS:
            return "No program headers found";
        case ELF_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case ELF_ERROR_INVALID_SEGMENT:
            return "Invalid segment";
        case ELF_ERROR_LOAD_FAILED:
            return "Load operation failed";
        case ELF_ERROR_NULL_POINTER:
            return "Null pointer argument";
        default:
            return "Unknown error";
    }
}

/**
 * Print ELF header information for debugging
 */
void elf_print_header_info(const Elf64_Ehdr* header) {
    if (header == NULL) {
        kprintf("ELF Header: NULL pointer\n");
        return;
    }

    kprintf("ELF Header Information:\n");
    kprintf("  Magic: %02x %02x %02x %02x\n", 
            header->e_ident[0], header->e_ident[1], 
            header->e_ident[2], header->e_ident[3]);
    kprintf("  Class: %s\n", (header->e_ident[4] == ELF_CLASS_64) ? "64-bit" : "32-bit");
    kprintf("  Data: %s\n", (header->e_ident[5] == ELF_DATA_LSB) ? "Little-endian" : "Big-endian");
    kprintf("  Version: %d\n", header->e_ident[6]);
    kprintf("  Type: %d\n", header->e_type);
    kprintf("  Machine: %d\n", header->e_machine);
    kprintf("  Entry point: 0x%lx\n", header->e_entry);
    kprintf("  Program headers: %d (offset 0x%lx)\n", header->e_phnum, header->e_phoff);
    kprintf("  Section headers: %d (offset 0x%lx)\n", header->e_shnum, header->e_shoff);
}

/**
 * Print program header information for debugging
 */
void elf_print_program_headers(const void* elf_data) {
    if (elf_data == NULL) {
        return;
    }

    const Elf64_Ehdr* header = (const Elf64_Ehdr*)elf_data;
    if (elf_validate_header(header) != ELF_OK) {
        return;
    }

    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((const uint8_t*)elf_data + header->e_phoff);

    kprintf("Program Headers:\n");
    kprintf("  Type     Offset   Virt Addr        Phys Addr        File Size    Mem Size     Flags  Align\n");

    for (int i = 0; i < header->e_phnum; i++) {
        const char* type_str;
        switch (phdr[i].p_type) {
            case PT_NULL:    type_str = "NULL";    break;
            case PT_LOAD:    type_str = "LOAD";    break;
            case PT_DYNAMIC: type_str = "DYNAMIC"; break;
            case PT_INTERP:  type_str = "INTERP";  break;
            case PT_NOTE:    type_str = "NOTE";    break;
            case PT_PHDR:    type_str = "PHDR";    break;
            case PT_TLS:     type_str = "TLS";     break;
            default:         type_str = "UNKNOWN"; break;
        }

        kprintf("  %-8s 0x%06lx 0x%016lx 0x%016lx 0x%08lx 0x%08lx %c%c%c    0x%lx\n",
                type_str, phdr[i].p_offset, phdr[i].p_vaddr, phdr[i].p_paddr,
                phdr[i].p_filesz, phdr[i].p_memsz,
                (phdr[i].p_flags & PF_R) ? 'R' : '-',
                (phdr[i].p_flags & PF_W) ? 'W' : '-',
                (phdr[i].p_flags & PF_X) ? 'X' : '-',
                phdr[i].p_align);
    }
}

/**
 * Check if ELF data represents a valid executable
 */
bool elf_is_valid_executable(const void* elf_data) {
    if (elf_data == NULL) {
        return false;
    }

    const Elf64_Ehdr* header = (const Elf64_Ehdr*)elf_data;
    return (elf_validate_header(header) == ELF_OK);
}

// Internal helper functions

/**
 * Check ELF magic number
 */
static bool elf_check_magic(const uint8_t* ident) {
    return (ident[0] == 0x7F && ident[1] == 'E' && ident[2] == 'L' && ident[3] == 'F');
}

/**
 * Setup memory layout for the ELF process
 */
static ElfStatus elf_setup_memory_layout(const void* elf_data, const Elf64_Ehdr* header, ElfProcess* process) {
    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((const uint8_t*)elf_data + header->e_phoff);
    uint64_t min_addr = UINT64_MAX;
    uint64_t max_addr = 0;

    // Calculate memory range
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t seg_start = phdr[i].p_vaddr;
            uint64_t seg_end = seg_start + phdr[i].p_memsz;

            if (seg_start < min_addr) {
                min_addr = seg_start;
            }
            if (seg_end > max_addr) {
                max_addr = seg_end;
            }
        }
    }

    if (min_addr == UINT64_MAX) {
        return ELF_ERROR_NO_PROGRAM_HEADERS;
    }

    // Set up process memory layout
    process->base_address = min_addr;
    process->total_size = max_addr - min_addr;
    
    // Set up heap and stack (simplified approach)
    process->heap_start = ELF_ALIGN_UP(max_addr, 0x1000);
    process->stack_start = 0x7FFFFF000000ULL;  // High address for stack

    return ELF_OK;
}

/**
 * Map all loadable segments of the ELF file
 */
static ElfStatus elf_map_segments(const void* elf_data, const Elf64_Ehdr* header, ElfProcess* process) {
    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((const uint8_t*)elf_data + header->e_phoff);
    ElfStatus status;

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            status = elf_load_segment(elf_data, &phdr[i], process->base_address);
            if (status != ELF_OK) {
                kprintf("Failed to load segment %d: %s\n", i, elf_get_error_string(status));
                return status;
            }
            
            kprintf("Loaded segment %d: vaddr=0x%lx, size=0x%lx, flags=%c%c%c\n",
                    i, phdr[i].p_vaddr, phdr[i].p_memsz,
                    (phdr[i].p_flags & PF_R) ? 'R' : '-',
                    (phdr[i].p_flags & PF_W) ? 'W' : '-',
                    (phdr[i].p_flags & PF_X) ? 'X' : '-');
        }
    }

    return ELF_OK;
}