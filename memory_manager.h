#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>
#include <stdbool.h>

// Bellek havuzu yapılandırma sabitleri
#define MM_POOL_INITIAL_SIZE (1024 * 1024)  // 1MB başlangıç boyutu
#define MM_BLOCK_MIN_SIZE 16                // Minimum blok boyutu
#define MM_MAX_POOLS 16                     // Maksimum havuz sayısı

// Hata kodları
typedef enum {
    MM_SUCCESS = 0,
    MM_ERROR_INIT_FAILED,
    MM_ERROR_INVALID_POINTER,
    MM_ERROR_OUT_OF_MEMORY,
    MM_ERROR_POOL_FULL
} mm_status_t;

// Bellek istatistikleri yapısı
typedef struct {
    size_t total_allocated;     // Toplam ayrılan bellek
    size_t current_used;        // Şu an kullanılan bellek
    size_t peak_used;          // En yüksek kullanım
    size_t total_allocations;  // Toplam allocation sayısı
    size_t total_frees;        // Toplam free sayısı
    double fragmentation;      // Fragmentasyon oranı (0.0 - 1.0)
} mm_stats_t;

// Ana fonksiyonlar
mm_status_t mm_init(void);
void* mm_alloc(size_t size);
mm_status_t mm_free(void* ptr);
void mm_cleanup(void);

// İstatistik ve debug fonksiyonları
mm_stats_t mm_get_stats(void);
void mm_print_stats(void);
bool mm_check_leaks(void);
void mm_debug_heap(void);

// Gelişmiş bellek yönetimi fonksiyonları
void* mm_aligned_alloc(size_t size, size_t alignment);
void* mm_realloc(void* ptr, size_t new_size);
void* mm_calloc(size_t num, size_t size);

// Havuz yönetimi fonksiyonları
int mm_create_pool(size_t size);
void mm_destroy_pool(int pool_id);
void* mm_pool_alloc(int pool_id, size_t size);
mm_status_t mm_pool_free(int pool_id, void* ptr);

#endif // MEMORY_MANAGER_H 