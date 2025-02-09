#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Bellek bloğu başlık yapısı
typedef struct block_header {
    size_t size;                    // Blok boyutu
    bool is_free;                   // Blok boş mu?
    struct block_header* next;      // Sonraki blok
    struct block_header* prev;      // Önceki blok
    uint32_t magic;                 // Güvenlik kontrolü için sihirli sayı
    uint32_t checksum;             // Veri bütünlüğü kontrolü
} block_header_t;

// Bellek havuzu yapısı
typedef struct {
    void* start;                    // Havuz başlangıç adresi
    size_t size;                    // Havuz boyutu
    block_header_t* first_block;    // İlk blok
    CRITICAL_SECTION mutex;         // Thread güvenliği için mutex
    bool is_initialized;            // Havuz başlatıldı mı?
} memory_pool_t;

// Global değişkenler
static memory_pool_t g_pools[MM_MAX_POOLS];
static CRITICAL_SECTION g_global_mutex;
static mm_stats_t g_stats = {0};
static bool g_is_initialized = false;

#define MAGIC_NUMBER 0xDEADBEEF
#define ALIGNMENT 8
#define HEADER_SIZE (sizeof(block_header_t))
#define MIN_ALLOC_SIZE (MM_BLOCK_MIN_SIZE + HEADER_SIZE)

// Yardımcı fonksiyonlar
static uint32_t calculate_checksum(block_header_t* header) {
    return (uint32_t)((uintptr_t)header ^ header->size ^ (header->is_free ? 1 : 0));
}

static bool validate_block(block_header_t* header) {
    if (!header) return false;
    if (header->magic != MAGIC_NUMBER) return false;
    return header->checksum == calculate_checksum(header);
}

static void update_block_metadata(block_header_t* header) {
    header->magic = MAGIC_NUMBER;
    header->checksum = calculate_checksum(header);
}

// Ana fonksiyonların implementasyonu
mm_status_t mm_init(void) {
    InitializeCriticalSection(&g_global_mutex);
    EnterCriticalSection(&g_global_mutex);
    
    if (g_is_initialized) {
        LeaveCriticalSection(&g_global_mutex);
        return MM_SUCCESS;
    }

    // İlk havuzu oluştur
    void* initial_memory = malloc(MM_POOL_INITIAL_SIZE);
    if (!initial_memory) {
        LeaveCriticalSection(&g_global_mutex);
        return MM_ERROR_INIT_FAILED;
    }

    g_pools[0].start = initial_memory;
    g_pools[0].size = MM_POOL_INITIAL_SIZE;
    g_pools[0].is_initialized = true;
    InitializeCriticalSection(&g_pools[0].mutex);

    // İlk bloğu ayarla
    block_header_t* first_block = (block_header_t*)initial_memory;
    first_block->size = MM_POOL_INITIAL_SIZE - HEADER_SIZE;
    first_block->is_free = true;
    first_block->next = NULL;
    first_block->prev = NULL;
    update_block_metadata(first_block);

    g_pools[0].first_block = first_block;
    g_is_initialized = true;

    LeaveCriticalSection(&g_global_mutex);
    return MM_SUCCESS;
}

void* mm_alloc(size_t size) {
    if (!g_is_initialized || size == 0) return NULL;

    // Boyutu alignment'a göre ayarla
    size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    if (size < MM_BLOCK_MIN_SIZE) size = MM_BLOCK_MIN_SIZE;

    EnterCriticalSection(&g_pools[0].mutex);

    block_header_t* current = g_pools[0].first_block;
    while (current) {
        if (!validate_block(current)) {
            LeaveCriticalSection(&g_pools[0].mutex);
            return NULL;
        }

        if (current->is_free && current->size >= size) {
            // Blok bölünebilir mi?
            if (current->size >= size + MIN_ALLOC_SIZE) {
                block_header_t* new_block = (block_header_t*)((char*)current + HEADER_SIZE + size);
                new_block->size = current->size - size - HEADER_SIZE;
                new_block->is_free = true;
                new_block->next = current->next;
                new_block->prev = current;
                update_block_metadata(new_block);

                current->size = size;
                current->next = new_block;
                update_block_metadata(current);
            }

            current->is_free = false;
            update_block_metadata(current);

            // İstatistikleri güncelle
            g_stats.total_allocated += size;
            g_stats.current_used += size;
            g_stats.total_allocations++;
            if (g_stats.current_used > g_stats.peak_used)
                g_stats.peak_used = g_stats.current_used;

            LeaveCriticalSection(&g_pools[0].mutex);
            return (void*)((char*)current + HEADER_SIZE);
        }
        current = current->next;
    }

    LeaveCriticalSection(&g_pools[0].mutex);
    return NULL;
}

mm_status_t mm_free(void* ptr) {
    if (!ptr || !g_is_initialized) return MM_ERROR_INVALID_POINTER;

    EnterCriticalSection(&g_pools[0].mutex);

    block_header_t* header = (block_header_t*)((char*)ptr - HEADER_SIZE);
    if (!validate_block(header)) {
        LeaveCriticalSection(&g_pools[0].mutex);
        return MM_ERROR_INVALID_POINTER;
    }

    size_t original_size = header->size;
    header->is_free = true;

    // Bitişik boş blokları birleştir
    size_t total_merged_size = original_size;

    if (header->next && header->next->is_free) {
        total_merged_size += HEADER_SIZE + header->next->size;
        header->size = total_merged_size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    if (header->prev && header->prev->is_free) {
        total_merged_size += HEADER_SIZE + header->prev->size;
        header->prev->size = total_merged_size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    } else {
        header->size = total_merged_size;
    }

    g_stats.current_used -= original_size;
    g_stats.total_frees++;

    update_block_metadata(header);
    LeaveCriticalSection(&g_pools[0].mutex);
    return MM_SUCCESS;
}

void mm_cleanup(void) {
    EnterCriticalSection(&g_global_mutex);
    
    if (!g_is_initialized) {
        LeaveCriticalSection(&g_global_mutex);
        return;
    }

    for (int i = 0; i < MM_MAX_POOLS; i++) {
        if (g_pools[i].is_initialized) {
            DeleteCriticalSection(&g_pools[i].mutex);
            free(g_pools[i].start);
            g_pools[i].is_initialized = false;
        }
    }

    memset(&g_stats, 0, sizeof(mm_stats_t));
    g_is_initialized = false;

    LeaveCriticalSection(&g_global_mutex);
    DeleteCriticalSection(&g_global_mutex);
}

mm_stats_t mm_get_stats(void) {
    mm_stats_t stats;
    EnterCriticalSection(&g_global_mutex);
    memcpy(&stats, &g_stats, sizeof(mm_stats_t));
    LeaveCriticalSection(&g_global_mutex);
    return stats;
}

void mm_print_stats(void) {
    mm_stats_t stats = mm_get_stats();
    printf("\n=== Memory Manager Statistics ===\n");
    printf("Total allocated memory: %I64u bytes\n", (uint64_t)stats.total_allocated);
    printf("Currently used memory: %I64u bytes\n", (uint64_t)stats.current_used);
    printf("Peak memory usage: %I64u bytes\n", (uint64_t)stats.peak_used);
    printf("Total allocations: %I64u\n", (uint64_t)stats.total_allocations);
    printf("Total frees: %I64u\n", (uint64_t)stats.total_frees);
    printf("Active allocations: %I64u\n", 
           (uint64_t)(stats.total_allocations - stats.total_frees));
    printf("==============================\n\n");
}

bool mm_check_leaks(void) {
    if (!g_is_initialized) return false;

    EnterCriticalSection(&g_global_mutex);
    bool has_leaks = (g_stats.total_allocations != g_stats.total_frees) || (g_stats.current_used != 0);
    LeaveCriticalSection(&g_global_mutex);

    return has_leaks;
} 