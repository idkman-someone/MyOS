#ifndef MYMAN_H
#define MYMAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "task.h"
#include "mm.h"

// MyMAN Package Manager for MyOS
// Version 1.0.0

#define MYMAN_VERSION_MAJOR 1
#define MYMAN_VERSION_MINOR 0
#define MYMAN_VERSION_PATCH 0
#define MYMAN_VERSION_STRING "1.0.0"

// Package manager constants
#define MYMAN_MAGIC         0x4D594D41  // 'MYMA' in ASCII
#define MYMAN_MAX_NAME_LEN  64
#define MYMAN_MAX_VERSION_LEN 16
#define MYMAN_MAX_DESC_LEN  256
#define MYMAN_MAX_DEPS      32
#define MYMAN_MAX_FILES     1024
#define MYMAN_MAX_PATH_LEN  512
#define MYMAN_CACHE_SIZE    128

// Repository and package paths
#define MYMAN_ROOT_DIR      "/system/packages"
#define MYMAN_CACHE_DIR     "/system/cache/myman"
#define MYMAN_DB_FILE       "/system/packages/myman.db"
#define MYMAN_REPO_LIST     "/system/packages/repositories.list"
#define MYMAN_CONFIG_FILE   "/etc/myman.conf"

// Package states
typedef enum {
    PKG_STATE_NOT_INSTALLED = 0,
    PKG_STATE_INSTALLED,
    PKG_STATE_PENDING_INSTALL,
    PKG_STATE_PENDING_REMOVE,
    PKG_STATE_BROKEN,
    PKG_STATE_HELD,
    PKG_STATE_UPGRADING
} package_state_t;

// Package priorities
typedef enum {
    PKG_PRIORITY_REQUIRED = 0,
    PKG_PRIORITY_IMPORTANT,
    PKG_PRIORITY_STANDARD,
    PKG_PRIORITY_OPTIONAL,
    PKG_PRIORITY_EXTRA
} package_priority_t;

// Architecture types
typedef enum {
    PKG_ARCH_ALL = 0,
    PKG_ARCH_X86_64,
    PKG_ARCH_I386,
    PKG_ARCH_ARM64,
    PKG_ARCH_ARMHF
} package_arch_t;

// Package dependency types
typedef enum {
    DEP_TYPE_DEPENDS = 0,
    DEP_TYPE_RECOMMENDS,
    DEP_TYPE_SUGGESTS,
    DEP_TYPE_CONFLICTS,
    DEP_TYPE_REPLACES,
    DEP_TYPE_PROVIDES
} dependency_type_t;

// Version comparison operators
typedef enum {
    VERSION_ANY = 0,
    VERSION_EQ,     // ==
    VERSION_NE,     // !=
    VERSION_LT,     // <
    VERSION_LE,     // <=
    VERSION_GT,     // >
    VERSION_GE      // >=
} version_op_t;

// MyMAN error codes
typedef enum {
    MYMAN_OK = 0,
    MYMAN_ERROR_INVALID_PACKAGE,
    MYMAN_ERROR_DEPENDENCY_CONFLICT,
    MYMAN_ERROR_ALREADY_INSTALLED,
    MYMAN_ERROR_NOT_INSTALLED,
    MYMAN_ERROR_DOWNLOAD_FAILED,
    MYMAN_ERROR_EXTRACT_FAILED,
    MYMAN_ERROR_PERMISSION_DENIED,
    MYMAN_ERROR_INSUFFICIENT_SPACE,
    MYMAN_ERROR_CORRUPTED_PACKAGE,
    MYMAN_ERROR_REPOSITORY_UNAVAILABLE,
    MYMAN_ERROR_PACKAGE_NOT_FOUND,
    MYMAN_ERROR_DATABASE_ERROR,
    MYMAN_ERROR_NETWORK_ERROR,
    MYMAN_ERROR_SIGNATURE_INVALID,
    MYMAN_ERROR_OUT_OF_MEMORY
} myman_error_t;

// Package version structure
typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t build;
    char suffix[16];  // For alpha, beta, rc, etc.
} package_version_t;

// Package dependency structure
typedef struct {
    char name[MYMAN_MAX_NAME_LEN];
    dependency_type_t type;
    version_op_t version_op;
    package_version_t version;
} package_dependency_t;

// Package file entry
typedef struct {
    char path[MYMAN_MAX_PATH_LEN];
    uint64_t size;
    uint32_t permissions;
    uint32_t checksum;
    bool is_config;
} package_file_t;

// Package metadata structure
typedef struct {
    // Header
    uint32_t magic;
    uint32_t version;
    
    // Basic package information
    char name[MYMAN_MAX_NAME_LEN];
    package_version_t version_info;
    char description[MYMAN_MAX_DESC_LEN];
    char maintainer[MYMAN_MAX_NAME_LEN];
    char homepage[MYMAN_MAX_PATH_LEN];
    
    // Package properties
    package_arch_t architecture;
    package_priority_t priority;
    package_state_t state;
    uint64_t installed_size;
    uint64_t download_size;
    
    // Dependencies
    uint32_t dependency_count;
    package_dependency_t dependencies[MYMAN_MAX_DEPS];
    
    // Files
    uint32_t file_count;
    package_file_t files[MYMAN_MAX_FILES];
    
    // Installation information
    uint64_t install_time;
    uint64_t last_modified;
    char install_reason[64];
    
    // Checksums and verification
    uint32_t package_checksum;
    uint32_t metadata_checksum;
    
    // Reserved for future use
    uint8_t reserved[256];
} __attribute__((packed)) package_metadata_t;

// Repository information
typedef struct {
    char name[MYMAN_MAX_NAME_LEN];
    char url[MYMAN_MAX_PATH_LEN];
    char public_key[256];
    bool enabled;
    uint32_t priority;
    uint64_t last_update;
} repository_t;

// Package cache entry
typedef struct package_cache_entry {
    char name[MYMAN_MAX_NAME_LEN];
    package_metadata_t metadata;
    struct package_cache_entry* next;
    uint64_t last_access;
} package_cache_entry_t;

// Installation transaction
typedef struct {
    uint32_t transaction_id;
    uint32_t package_count;
    char packages[MYMAN_MAX_DEPS][MYMAN_MAX_NAME_LEN];
    bool dry_run;
    bool force;
    bool auto_confirm;
} install_transaction_t;

// MyMAN context structure
typedef struct {
    // Configuration
    char root_dir[MYMAN_MAX_PATH_LEN];
    char cache_dir[MYMAN_MAX_PATH_LEN];
    bool auto_remove_orphans;
    bool check_signatures;
    uint32_t max_concurrent_downloads;
    
    // Database
    package_cache_entry_t* package_cache;
    uint32_t cache_entries;
    
    // Repositories
    repository_t repositories[32];
    uint32_t repository_count;
    
    // Statistics
    uint64_t packages_installed;
    uint64_t packages_available;
    uint64_t total_installed_size;
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    // Locks and synchronization
    mutex_t database_mutex;
    mutex_t cache_mutex;
    bool maintenance_mode;
} myman_context_t;

// Function prototypes

// Core MyMAN functions
myman_error_t myman_init(myman_context_t* ctx);
void myman_cleanup(myman_context_t* ctx);
const char* myman_get_error_string(myman_error_t error);

// Package management operations
myman_error_t myman_install_package(myman_context_t* ctx, const char* package_name);
myman_error_t myman_remove_package(myman_context_t* ctx, const char* package_name);
myman_error_t myman_upgrade_package(myman_context_t* ctx, const char* package_name);
myman_error_t myman_upgrade_all(myman_context_t* ctx);
myman_error_t myman_autoremove(myman_context_t* ctx);

// Package search and information
myman_error_t myman_search_packages(myman_context_t* ctx, const char* pattern, 
                                   package_metadata_t** results, uint32_t* count);
myman_error_t myman_show_package(myman_context_t* ctx, const char* package_name, 
                                package_metadata_t* metadata);
myman_error_t myman_list_installed(myman_context_t* ctx, package_metadata_t** packages, 
                                  uint32_t* count);
myman_error_t myman_list_upgradable(myman_context_t* ctx, package_metadata_t** packages, 
                                   uint32_t* count);

// Dependency resolution
myman_error_t myman_resolve_dependencies(myman_context_t* ctx, const char* package_name,
                                        char** install_list, uint32_t* install_count,
                                        char** remove_list, uint32_t* remove_count);
bool myman_check_dependency_satisfied(myman_context_t* ctx, const package_dependency_t* dep);

// Repository management
myman_error_t myman_add_repository(myman_context_t* ctx, const char* name, const char* url);
myman_error_t myman_remove_repository(myman_context_t* ctx, const char* name);
myman_error_t myman_update_repositories(myman_context_t* ctx);
myman_error_t myman_list_repositories(myman_context_t* ctx, repository_t** repos, uint32_t* count);

// Package file operations
myman_error_t myman_extract_package(const char* package_file, const char* dest_dir);
myman_error_t myman_create_package(const char* source_dir, const char* package_file,
                                  const package_metadata_t* metadata);
myman_error_t myman_verify_package(const char* package_file);

// Database operations
myman_error_t myman_load_database(myman_context_t* ctx);
myman_error_t myman_save_database(myman_context_t* ctx);
myman_error_t myman_rebuild_database(myman_context_t* ctx);

// Cache management
myman_error_t myman_cache_package(myman_context_t* ctx, const package_metadata_t* metadata);
package_metadata_t* myman_get_cached_package(myman_context_t* ctx, const char* name);
void myman_clear_cache(myman_context_t* ctx);

// Version comparison utilities
int myman_compare_versions(const package_version_t* v1, const package_version_t* v2);
bool myman_version_satisfies(const package_version_t* version, 
                            version_op_t op, const package_version_t* required);
myman_error_t myman_parse_version_string(const char* version_str, package_version_t* version);
void myman_version_to_string(const package_version_t* version, char* buffer, size_t size);

// Configuration management
myman_error_t myman_load_config(myman_context_t* ctx, const char* config_file);
myman_error_t myman_save_config(myman_context_t* ctx, const char* config_file);

// Utility functions
uint32_t myman_calculate_checksum(const void* data, size_t size);
bool myman_verify_checksum(const void* data, size_t size, uint32_t expected);
myman_error_t myman_download_file(const char* url, const char* dest_path);
bool myman_file_exists(const char* path);
uint64_t myman_get_file_size(const char* path);
uint64_t myman_get_free_space(const char* path);

// Package format utilities
myman_error_t myman_read_package_metadata(const char* package_file, package_metadata_t* metadata);
myman_error_t myman_write_package_metadata(const char* package_file, const package_metadata_t* metadata);

// Transaction management
myman_error_t myman_begin_transaction(myman_context_t* ctx, install_transaction_t* transaction);
myman_error_t myman_commit_transaction(myman_context_t* ctx, const install_transaction_t* transaction);
myman_error_t myman_rollback_transaction(myman_context_t* ctx, const install_transaction_t* transaction);

// Logging and debugging
void myman_log(const char* level, const char* format, ...);
void myman_debug(const char* format, ...);
void myman_print_package_info(const package_metadata_t* metadata);
void myman_print_statistics(const myman_context_t* ctx);

// Command-line interface functions
int myman_cmd_install(int argc, char** argv);
int myman_cmd_remove(int argc, char** argv);
int myman_cmd_search(int argc, char** argv);
int myman_cmd_show(int argc, char** argv);
int myman_cmd_list(int argc, char** argv);
int myman_cmd_update(int argc, char** argv);
int myman_cmd_upgrade(int argc, char** argv);
int myman_cmd_autoremove(int argc, char** argv);
int myman_cmd_clean(int argc, char** argv);

// Global MyMAN context (singleton)
extern myman_context_t* g_myman_ctx;

// Initialization macros
#define MYMAN_INIT_CONTEXT() do { \
    if (!g_myman_ctx) { \
        g_myman_ctx = kmalloc(sizeof(myman_context_t)); \
        if (g_myman_ctx) myman_init(g_myman_ctx); \
    } \
} while(0)

#define MYMAN_CHECK_INIT() do { \
    if (!g_myman_ctx) return MYMAN_ERROR_DATABASE_ERROR; \
} while(0)

#endif // MYMAN_H