#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <stdbool.h>

// ELF64 Header Structure
typedef struct {
    uint8_t  e_ident[16];     // ELF identification
    uint16_t e_type;          // Object file type
    uint16_t e_machine;       // Machine type
    uint32_t e_version;       // Object file version
    uint64_t e_entry;         // Entry point address
    uint64_t e_phoff;         // Program header offset
    uint64_t e_shoff;         // Section header offset
    uint32_t e_flags;         // Processor-specific flags
    uint16_t e_ehsize;        // ELF header size
    uint16_t e_phentsize;     // Program header entry size
    uint16_t e_phnum;         // Number of program header entries
    uint16_t e_shentsize;     // Section header entry size
    uint16_t e_shnum;         // Number of section header entries
    uint16_t e_shstrndx;      // Section header string table index
} __attribute__((packed)) Elf64_Ehdr;

// ELF64 Program Header Structure
typedef struct {
    uint32_t p_type;          // Segment type
    uint32_t p_flags;         // Segment flags
    uint64_t p_offset;        // Segment file offset
    uint64_t p_vaddr;         // Segment virtual address
    uint64_t p_paddr;         // Segment physical address
    uint64_t p_filesz;        // Segment size in file
    uint64_t p_memsz;         // Segment size in memory
    uint64_t p_align;         // Segment alignment
} __attribute__((packed)) Elf64_Phdr;

// ELF64 Section Header Structure
typedef struct {
    uint32_t sh_name;         // Section name (string table index)
    uint32_t sh_type;         // Section type
    uint64_t sh_flags;        // Section flags
    uint64_t sh_addr;         // Section virtual address
    uint64_t sh_offset;       // Section file offset
    uint64_t sh_size;         // Section size
    uint32_t sh_link;         // Link to another section
    uint32_t sh_info;         // Additional section information
    uint64_t sh_addralign;    // Section alignment
    uint64_t sh_entsize;      // Entry size if section holds table
} __attribute__((packed)) Elf64_Shdr;

// ELF Magic Numbers and Constants
#define ELF_MAGIC       0x7F454C46  // "\x7FELF"
#define ELF_CLASS_64    2           // 64-bit objects
#define ELF_DATA_LSB    1           // Little-endian
#define ELF_VERSION     1           // Current version

// ELF Types
#define ET_NONE         0           // No file type
#define ET_REL          1           // Relocatable file
#define ET_EXEC         2           // Executable file
#define ET_DYN          3           // Shared object file
#define ET_CORE         4           // Core file

// Machine Types
#define EM_X86_64       62          // AMD x86-64 architecture

// Program Header Types
#define PT_NULL         0           // Unused segment
#define PT_LOAD         1           // Loadable segment
#define PT_DYNAMIC      2           // Dynamic linking information
#define PT_INTERP       3           // Interpreter information
#define PT_NOTE         4           // Auxiliary information
#define PT_SHLIB        5           // Reserved
#define PT_PHDR         6           // Program header table
#define PT_TLS          7           // Thread-local storage

// Program Header Flags
#define PF_X            0x1         // Execute
#define PF_W            0x2         // Write
#define PF_R            0x4         // Read

// Section Types
#define SHT_NULL        0           // Inactive section
#define SHT_PROGBITS    1           // Program-defined information
#define SHT_SYMTAB      2           // Symbol table
#define SHT_STRTAB      3           // String table
#define SHT_RELA        4           // Relocation entries with addends
#define SHT_HASH        5           // Symbol hash table
#define SHT_DYNAMIC     6           // Dynamic linking information
#define SHT_NOTE        7           // Notes
#define SHT_NOBITS      8           // Program space with no data (bss)
#define SHT_REL         9           // Relocation entries, no addends
#define SHT_SHLIB       10          // Reserved
#define SHT_DYNSYM      11          // Dynamic linker symbol table

// Section Flags
#define SHF_WRITE       0x1         // Writable
#define SHF_ALLOC       0x2         // Occupies memory during execution
#define SHF_EXECINSTR   0x4         // Executable

// ELF Loader Status Codes
typedef enum {
    ELF_OK = 0,
    ELF_ERROR_INVALID_MAGIC,
    ELF_ERROR_INVALID_CLASS,
    ELF_ERROR_INVALID_ENDIAN,
    ELF_ERROR_INVALID_VERSION,
    ELF_ERROR_INVALID_TYPE,
    ELF_ERROR_INVALID_MACHINE,
    ELF_ERROR_NO_PROGRAM_HEADERS,
    ELF_ERROR_MEMORY_ALLOCATION,
    ELF_ERROR_INVALID_SEGMENT,
    ELF_ERROR_LOAD_FAILED,
    ELF_ERROR_NULL_POINTER
} ElfStatus;

// ELF Process Structure
typedef struct {
    uint64_t entry_point;     // Program entry point
    uint64_t base_address;    // Base virtual address
    uint64_t heap_start;      // Heap start address
    uint64_t stack_start;     // Stack start address
    size_t   total_size;      // Total memory size required
    bool     is_loaded;       // Load status
} ElfProcess;

// Function Prototypes
ElfStatus elf_validate_header(const Elf64_Ehdr* header);
ElfStatus elf_load_from_memory(const void* elf_data, size_t size, ElfProcess* process);
ElfStatus elf_load_from_file(const char* filename, ElfProcess* process);
ElfStatus elf_load_segment(const void* elf_data, const Elf64_Phdr* phdr, uint64_t base_addr);
uint64_t elf_calculate_load_size(const void* elf_data);
const char* elf_get_error_string(ElfStatus status);
void elf_print_header_info(const Elf64_Ehdr* header);
void elf_print_program_headers(const void* elf_data);
bool elf_is_valid_executable(const void* elf_data);

// Utility Macros
#define ELF_ALIGN_UP(addr, align)   (((addr) + (align) - 1) & ~((align) - 1))
#define ELF_ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))

#endif // ELF_LOADER_H