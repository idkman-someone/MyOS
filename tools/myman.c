[file name]: myman.c
[file content begin]
#include "myman.h"
#include "kernel.h"
#include "console.h"
#include "mm.h"
#include "task.h"
#include <stddef.h>
#include <string.h>

// Global MyMAN context
myman_context_t* g_myman_ctx = NULL;

// Error message strings
static const char* error_messages[] = {
    "Success",
    "Invalid package",
    "Dependency conflict",
    "Package already installed",
    "Package not installed",
    "Download failed",
    "Extract failed",
    "Permission denied",
    "Insufficient disk space",
    "Corrupted package",
    "Repository unavailable",
    "Package not found",
    "Database error",
    "Network error",
    "Invalid signature",
    "Out of memory"
};

// Helper function to copy strings safely
static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    for (size_t i = 0; i < copy_len; i++) {
        dest[i] = src[i];
    }
    dest[copy_len] = '\0';
}

// Helper function to compare strings
static int safe_strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return s1 == s2 ? 0 : (s1 ? 1 : -1);
    
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Initialize MyMAN context
myman_error_t myman_init(myman_context_t* ctx) {
    if (!ctx) return MYMAN_ERROR_INVALID_PACKAGE;
    
    // Set default configuration
    safe_strcpy(ctx->root_dir, MYMAN_ROOT_DIR, sizeof(ctx->root_dir));
    safe_strcpy(ctx->cache_dir, MYMAN_CACHE_DIR, sizeof(ctx->cache_dir));
    ctx->auto_remove_orphans = true;
    ctx->check_signatures = true;
    ctx->max_concurrent_downloads = 4;
    
    // Initialize mutexes
    ctx->database_mutex = mutex_create();
    ctx->cache_mutex = mutex_create();
    if (!ctx->database_mutex || !ctx->cache_mutex) {
        return MYMAN_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize cache
    ctx->package_cache = NULL;
    ctx->cache_entries = 0;
    ctx->repository_count = 0;
    ctx->packages_installed = 0;
    ctx->packages_available = 0;
    ctx->total_installed_size = 0;
    ctx->cache_hits = 0;
    ctx->cache_misses = 0;
    ctx->maintenance_mode = false;
    
    // Load configuration
    myman_error_t err = myman_load_config(ctx, MYMAN_CONFIG_FILE);
    if (err != MYMAN_OK) {
        kprintf("Warning: Failed to load config: %d\n", err);
    }
    
    // Load database
    err = myman_load_database(ctx);
    if (err != MYMAN_OK) {
        kprintf("Failed to load database: %d\n", err);
        return err;
    }
    
    // Add default repository
    myman_add_repository(ctx, "main", "https://repo.myos.org/main");
    
    return MYMAN_OK;
}

// Cleanup resources
void myman_cleanup(myman_context_t* ctx) {
    if (!ctx) return;
    
    // Save database and config
    myman_save_database(ctx);
    myman_save_config(ctx, MYMAN_CONFIG_FILE);
    
    // Clear cache
    myman_clear_cache(ctx);
    
    // Destroy mutexes
    if (ctx->database_mutex) mutex_destroy(ctx->database_mutex);
    if (ctx->cache_mutex) mutex_destroy(ctx->cache_mutex);
}

// Get error string
const char* myman_get_error_string(myman_error_t error) {
    if (error < 0 || error >= (sizeof(error_messages)/sizeof(error_messages[0])) {
        return "Unknown error";
    }
    return error_messages[error];
}

// Package installation
myman_error_t myman_install_package(myman_context_t* ctx, const char* package_name) {
    MYMAN_CHECK_INIT();
    mutex_lock(ctx->database_mutex);
    
    // Check if package is already installed
    package_metadata_t* installed = myman_get_cached_package(ctx, package_name);
    if (installed && installed->state == PKG_STATE_INSTALLED) {
        mutex_unlock(ctx->database_mutex);
        return MYMAN_ERROR_ALREADY_INSTALLED;
    }
    
    // Resolve dependencies
    char* install_list[MYMAN_MAX_DEPS];
    char* remove_list[MYMAN_MAX_DEPS];
    uint32_t install_count = 0;
    uint32_t remove_count = 0;
    
    myman_error_t err = myman_resolve_dependencies(ctx, package_name, install_list, &install_count, 
                                                  remove_list, &remove_count);
    if (err != MYMAN_OK) {
        mutex_unlock(ctx->database_mutex);
        return err;
    }
    
    // Create transaction
    install_transaction_t trans;
    trans.transaction_id = 0; // TODO: Generate unique ID
    trans.package_count = install_count;
    trans.dry_run = false;
    trans.force = false;
    trans.auto_confirm = true;
    
    for (uint32_t i = 0; i < install_count; i++) {
        safe_strcpy(trans.packages[i], install_list[i], MYMAN_MAX_NAME_LEN);
    }
    
    // Execute transaction
    err = myman_begin_transaction(ctx, &trans);
    if (err != MYMAN_OK) {
        mutex_unlock(ctx->database_mutex);
        return err;
    }
    
    err = myman_commit_transaction(ctx, &trans);
    mutex_unlock(ctx->database_mutex);
    return err;
}

// Package removal
myman_error_t myman_remove_package(myman_context_t* ctx, const char* package_name) {
    MYMAN_CHECK_INIT();
    mutex_lock(ctx->database_mutex);
    
    // Check if package is installed
    package_metadata_t* pkg = myman_get_cached_package(ctx, package_name);
    if (!pkg || pkg->state != PKG_STATE_INSTALLED) {
        mutex_unlock(ctx->database_mutex);
        return MYMAN_ERROR_NOT_INSTALLED;
    }
    
    // Check if other packages depend on this one
    for (int i = 0; i < ctx->cache_entries; i++) {
        package_cache_entry_t* entry = &ctx->package_cache[i];
        if (entry->metadata.state == PKG_STATE_INSTALLED) {
            for (int j = 0; j < entry->metadata.dependency_count; j++) {
                if (safe_strcmp(entry->metadata.dependencies[j].name, package_name) == 0 &&
                    entry->metadata.dependencies[j].type == DEP_TYPE_DEPENDS) {
                    mutex_unlock(ctx->database_mutex);
                    return MYMAN_ERROR_DEPENDENCY_CONFLICT;
                }
            }
        }
    }
    
    // Create transaction
    install_transaction_t trans;
    trans.transaction_id = 0;
    trans.package_count = 1;
    trans.dry_run = false;
    trans.force = false;
    trans.auto_confirm = true;
    safe_strcpy(trans.packages[0], package_name, MYMAN_MAX_NAME_LEN);
    
    myman_error_t err = myman_begin_transaction(ctx, &trans);
    if (err == MYMAN_OK) {
        err = myman_commit_transaction(ctx, &trans);
    }
    
    mutex_unlock(ctx->database_mutex);
    return err;
}

// Upgrade package
myman_error_t myman_upgrade_package(myman_context_t* ctx, const char* package_name) {
    // Implementation would:
    // 1. Check for newer version
    // 2. Resolve dependencies
    // 3. Create upgrade transaction
    return MYMAN_OK;
}

// Upgrade all packages
myman_error_t myman_upgrade_all(myman_context_t* ctx) {
    // Implementation would:
    // 1. Find all upgradable packages
    // 2. Create bulk transaction
    return MYMAN_OK;
}

// Auto-remove orphaned packages
myman_error_t myman_autoremove(myman_context_t* ctx) {
    // Implementation would:
    // 1. Find packages installed as dependencies that are no longer needed
    // 2. Remove them
    return MYMAN_OK;
}

// Search packages
myman_error_t myman_search_packages(myman_context_t* ctx, const char* pattern, 
                                   package_metadata_t** results, uint32_t* count) {
    // Implementation would search package names and descriptions
    return MYMAN_OK;
}

// Show package information
myman_error_t myman_show_package(myman_context_t* ctx, const char* package_name, 
                                package_metadata_t* metadata) {
    MYMAN_CHECK_INIT();
    mutex_lock(ctx->cache_mutex);
    
    package_metadata_t* cached = myman_get_cached_package(ctx, package_name);
    if (!cached) {
        mutex_unlock(ctx->cache_mutex);
        return MYMAN_ERROR_PACKAGE_NOT_FOUND;
    }
    
    *metadata = *cached;
    mutex_unlock(ctx->cache_mutex);
    return MYMAN_OK;
}

// List installed packages
myman_error_t myman_list_installed(myman_context_t* ctx, package_metadata_t** packages, 
                                  uint32_t* count) {
    // Implementation would return all installed packages
    return MYMAN_OK;
}

// Add repository
myman_error_t myman_add_repository(myman_context_t* ctx, const char* name, const char* url) {
    if (!ctx || !name || !url || ctx->repository_count >= 32) {
        return MYMAN_ERROR_INVALID_PACKAGE;
    }
    
    repository_t* repo = &ctx->repositories[ctx->repository_count++];
    safe_strcpy(repo->name, name, MYMAN_MAX_NAME_LEN);
    safe_strcpy(repo->url, url, MYMAN_MAX_PATH_LEN);
    repo->enabled = true;
    repo->priority = 500;
    repo->last_update = 0;
    return MYMAN_OK;
}

// Version comparison
int myman_compare_versions(const package_version_t* v1, const package_version_t* v2) {
    if (v1->major != v2->major) 
        return v1->major > v2->major ? 1 : -1;
    if (v1->minor != v2->minor) 
        return v1->minor > v2->minor ? 1 : -1;
    if (v1->patch != v2->patch) 
        return v1->patch > v2->patch ? 1 : -1;
    if (v1->build != v2->build) 
        return v1->build > v2->build ? 1 : -1;
    
    // Compare suffixes lexicographically
    int suffix_cmp = safe_strcmp(v1->suffix, v2->suffix);
    if (suffix_cmp != 0)
        return suffix_cmp > 0 ? 1 : -1;
    
    return 0;
}

// Load database
myman_error_t myman_load_database(myman_context_t* ctx) {
    if (!ctx) return MYMAN_ERROR_INVALID_PACKAGE;
    
    // Check if database file exists
    if (!myman_file_exists(MYMAN_DB_FILE)) {
        return MYMAN_OK; // No database is acceptable for first run
    }
    
    // Implementation would include:
    // 1. Open DB file
    // 2. Read package metadata
    // 3. Populate cache
    // 4. Verify checksums
    
    return MYMAN_OK;
}

// Save database
myman_error_t myman_save_database(myman_context_t* ctx) {
    // Implementation would write package database
    return MYMAN_OK;
}

// Cache package metadata
myman_error_t myman_cache_package(myman_context_t* ctx, const package_metadata_t* metadata) {
    MYMAN_CHECK_INIT();
    mutex_lock(ctx->cache_mutex);
    
    // Check if already cached
    package_cache_entry_t* entry = myman_get_cached_package(ctx, metadata->name);
    if (entry) {
        entry->metadata = *metadata;
        entry->last_access = ktime_get();
        mutex_unlock(ctx->cache_mutex);
        return MYMAN_OK;
    }
    
    // Create new cache entry
    if (ctx->cache_entries >= MYMAN_CACHE_SIZE) {
        // Evict least recently used
        package_cache_entry_t* lru = ctx->package_cache;
        for (package_cache_entry_t* e = ctx->package_cache; e; e = e->next) {
            if (e->last_access < lru->last_access) lru = e;
        }
        lru->metadata = *metadata;
        lru->last_access = ktime_get();
        mutex_unlock(ctx->cache_mutex);
        return MYMAN_OK;
    }
    
    // Allocate new entry
    package_cache_entry_t* new_entry = kmalloc(sizeof(package_cache_entry_t));
    if (!new_entry) {
        mutex_unlock(ctx->cache_mutex);
        return MYMAN_ERROR_OUT_OF_MEMORY;
    }
    
    new_entry->metadata = *metadata;
    new_entry->last_access = ktime_get();
    safe_strcpy(new_entry->name, metadata->name, MYMAN_MAX_NAME_LEN);
    
    // Add to cache
    new_entry->next = ctx->package_cache;
    ctx->package_cache = new_entry;
    ctx->cache_entries++;
    
    mutex_unlock(ctx->cache_mutex);
    return MYMAN_OK;
}

// Get cached package
package_metadata_t* myman_get_cached_package(myman_context_t* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    
    mutex_lock(ctx->cache_mutex);
    package_cache_entry_t* entry = ctx->package_cache;
    while (entry) {
        if (safe_strcmp(entry->name, name) == 0) {
            entry->last_access = ktime_get();
            mutex_unlock(ctx->cache_mutex);
            return &entry->metadata;
        }
        entry = entry->next;
    }
    mutex_unlock(ctx->cache_mutex);
    return NULL;
}

// Clear cache
void myman_clear_cache(myman_context_t* ctx) {
    if (!ctx) return;
    
    mutex_lock(ctx->cache_mutex);
    package_cache_entry_t* entry = ctx->package_cache;
    while (entry) {
        package_cache_entry_t* next = entry->next;
        kfree(entry);
        entry = next;
    }
    ctx->package_cache = NULL;
    ctx->cache_entries = 0;
    mutex_unlock(ctx->cache_mutex);
}

// Version parsing
myman_error_t myman_parse_version_string(const char* version_str, package_version_t* version) {
    if (!version_str || !version) return MYMAN_ERROR_INVALID_PACKAGE;
    
    memset(version, 0, sizeof(package_version_t));
    int count = sscanf(version_str, "%u.%u.%u.%u", 
                      &version->major, &version->minor, 
                      &version->patch, &version->build);
    
    if (count < 3) return MYMAN_ERROR_INVALID_PACKAGE;
    
    // Extract suffix if present
    const char* suffix = strchr(version_str, '-');
    if (suffix) {
        safe_strcpy(version->suffix, suffix + 1, sizeof(version->suffix));
    }
    
    return MYMAN_OK;
}

// Version to string
void myman_version_to_string(const package_version_t* version, char* buffer, size_t size) {
    if (!version || !buffer || size == 0) return;
    
    if (version->suffix[0]) {
        snprintf(buffer, size, "%u.%u.%u.%u-%s", 
                version->major, version->minor, 
                version->patch, version->build, 
                version->suffix);
    } else {
        snprintf(buffer, size, "%u.%u.%u.%u", 
                version->major, version->minor, 
                version->patch, version->build);
    }
}

// Begin transaction
myman_error_t myman_begin_transaction(myman_context_t* ctx, install_transaction_t* transaction) {
    // Implementation would:
    // 1. Verify packages exist
    // 2. Check dependencies
    // 3. Create rollback point
    return MYMAN_OK;
}

// Commit transaction
myman_error_t myman_commit_transaction(myman_context_t* ctx, const install_transaction_t* transaction) {
    // Implementation would:
    // 1. Download packages
    // 2. Extract files
    // 3. Update database
    return MYMAN_OK;
}

// Rollback transaction
myman_error_t myman_rollback_transaction(myman_context_t* ctx, const install_transaction_t* transaction) {
    // Implementation would revert all changes
    return MYMAN_OK;
}

// CLI: Install command
int myman_cmd_install(int argc, char** argv) {
    MYMAN_INIT_CONTEXT();
    if (argc < 2) {
        kprintf("Usage: myman install <package>\n");
        return -1;
    }
    
    for (int i = 1; i < argc; i++) {
        myman_error_t err = myman_install_package(g_myman_ctx, argv[i]);
        if (err != MYMAN_OK) {
            kprintf("Failed to install %s: %s\n", argv[i], myman_get_error_string(err));
            return -2;
        }
        kprintf("Successfully installed %s\n", argv[i]);
    }
    return 0;
}

// CLI: Remove command
int myman_cmd_remove(int argc, char** argv) {
    MYMAN_INIT_CONTEXT();
    if (argc < 2) {
        kprintf("Usage: myman remove <package>\n");
        return -1;
    }
    
    for (int i = 1; i < argc; i++) {
        myman_error_t err = myman_remove_package(g_myman_ctx, argv[i]);
        if (err != MYMAN_OK) {
            kprintf("Failed to remove %s: %s\n", argv[i], myman_get_error_string(err));
            return -2;
        }
        kprintf("Successfully removed %s\n", argv[i]);
    }
    return 0;
}

// Main entry point
int myman_main(int argc, char** argv) {
    if (argc < 2) {
        kprintf("MyMAN Package Manager v%s\n", MYMAN_VERSION_STRING);
        kprintf("Usage: myman <command> [options] [packages]\n");
        kprintf("Commands:\n");
        kprintf("  install <pkgs>  Install packages\n");
        kprintf("  remove <pkgs>   Remove packages\n");
        kprintf("  search <term>   Search for packages\n");
        kprintf("  show <pkg>      Show package information\n");
        kprintf("  list            List installed packages\n");
        kprintf("  update          Update package lists\n");
        kprintf("  upgrade         Upgrade all packages\n");
        kprintf("  autoremove      Remove orphaned packages\n");
        kprintf("  clean           Clear package cache\n");
        return 0;
    }
    
    const char* command = argv[1];
    
    if (safe_strcmp(command, "install") == 0) {
        return myman_cmd_install(argc - 1, argv + 1);
    }
    else if (safe_strcmp(command, "remove") == 0) {
        return myman_cmd_remove(argc - 1, argv + 1);
    }
    else if (safe_strcmp(command, "search") == 0) {
        // Implementation for search
    }
    else if (safe_strcmp(command, "show") == 0) {
        // Implementation for show
    }
    else if (safe_strcmp(command, "list") == 0) {
        // Implementation for list
    }
    else if (safe_strcmp(command, "update") == 0) {
        myman_update_repositories(g_myman_ctx);
    }
    else if (safe_strcmp(command, "upgrade") == 0) {
        myman_upgrade_all(g_myman_ctx);
    }
    else if (safe_strcmp(command, "autoremove") == 0) {
        myman_autoremove(g_myman_ctx);
    }
    else if (safe_strcmp(command, "clean") == 0) {
        myman_clear_cache(g_myman_ctx);
    }
    else {
        kprintf("Unknown command: %s\n", command);
        return -1;
    }
    
    return 0;
}
[file content end]