#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

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
    pthread_mutex_t mutex;          // Thread güvenliği için mutex
    bool is_initialized;            // Havuz başlatıldı mı?
} memory_pool_t;

// Global değişkenler
static memory_pool_t g_pools[MM_MAX_POOLS];
static pthread_mutex_t g_global_mutex = PTHREAD_MUTEX_INITIALIZER;
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
    pthread_mutex_lock(&g_global_mutex);
    
    if (g_is_initialized) {
        pthread_mutex_unlock(&g_global_mutex);
        return MM_SUCCESS;
    }

    // İlk havuzu oluştur
    void* initial_memory = malloc(MM_POOL_INITIAL_SIZE);
    if (!initial_memory) {
        pthread_mutex_unlock(&g_global_mutex);
        return MM_ERROR_INIT_FAILED;
    }

    g_pools[0].start = initial_memory;
    g_pools[0].size = MM_POOL_INITIAL_SIZE;
    g_pools[0].is_initialized = true;
    pthread_mutex_init(&g_pools[0].mutex, NULL);

    // İlk bloğu ayarla
    block_header_t* first_block = (block_header_t*)initial_memory;
    first_block->size = MM_POOL_INITIAL_SIZE - HEADER_SIZE;
    first_block->is_free = true;
    first_block->next = NULL;
    first_block->prev = NULL;
    update_block_metadata(first_block);

    g_pools[0].first_block = first_block;
    g_is_initialized = true;

    pthread_mutex_unlock(&g_global_mutex);
    return MM_SUCCESS;
}

void* mm_alloc(size_t size) {
    if (!g_is_initialized || size == 0) return NULL;

    // Boyutu alignment'a göre ayarla
    size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    if (size < MM_BLOCK_MIN_SIZE) size = MM_BLOCK_MIN_SIZE;

    pthread_mutex_lock(&g_pools[0].mutex);

    block_header_t* current = g_pools[0].first_block;
    while (current) {
        if (!validate_block(current)) {
            pthread_mutex_unlock(&g_pools[0].mutex);
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

            pthread_mutex_unlock(&g_pools[0].mutex);
            return (void*)((char*)current + HEADER_SIZE);
        }
        current = current->next;
    }

    pthread_mutex_unlock(&g_pools[0].mutex);
    return NULL;
}

mm_status_t mm_free(void* ptr) {
    if (!ptr || !g_is_initialized) return MM_ERROR_INVALID_POINTER;

    pthread_mutex_lock(&g_pools[0].mutex);

    block_header_t* header = (block_header_t*)((char*)ptr - HEADER_SIZE);
    if (!validate_block(header)) {
        pthread_mutex_unlock(&g_pools[0].mutex);
        return MM_ERROR_INVALID_POINTER;
    }

    header->is_free = true;
    g_stats.current_used -= header->size;
    g_stats.total_frees++;

    // Bitişik boş blokları birleştir
    if (header->next && header->next->is_free) {
        header->size += HEADER_SIZE + header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    if (header->prev && header->prev->is_free) {
        header->prev->size += HEADER_SIZE + header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    }

    update_block_metadata(header);
    pthread_mutex_unlock(&g_pools[0].mutex);
    return MM_SUCCESS;
}

void mm_cleanup(void) {
    pthread_mutex_lock(&g_global_mutex);
    
    if (!g_is_initialized) {
        pthread_mutex_unlock(&g_global_mutex);
        return;
    }

    for (int i = 0; i < MM_MAX_POOLS; i++) {
        if (g_pools[i].is_initialized) {
            pthread_mutex_destroy(&g_pools[i].mutex);
            free(g_pools[i].start);
            g_pools[i].is_initialized = false;
        }
    }

    memset(&g_stats, 0, sizeof(mm_stats_t));
    g_is_initialized = false;

    pthread_mutex_unlock(&g_global_mutex);
}

mm_stats_t mm_get_stats(void) {
    mm_stats_t stats;
    pthread_mutex_lock(&g_global_mutex);
    memcpy(&stats, &g_stats, sizeof(mm_stats_t));
    pthread_mutex_unlock(&g_global_mutex);
    return stats;
}

void mm_print_stats(void) {
    mm_stats_t stats = mm_get_stats();
    printf("\n=== Bellek Yönetici İstatistikleri ===\n");
    printf("Toplam ayrılan bellek: %zu bytes\n", stats.total_allocated);
    printf("Şu an kullanılan bellek: %zu bytes\n", stats.current_used);
    printf("En yüksek kullanım: %zu bytes\n", stats.peak_used);
    printf("Toplam allocation sayısı: %zu\n", stats.total_allocations);
    printf("Toplam free sayısı: %zu\n", stats.total_frees);
    printf("Aktif allocation sayısı: %zu\n", stats.total_allocations - stats.total_frees);
    printf("===================================\n\n");
} 