#include "memory_manager.h"
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#define NUM_THREADS 4
#define NUM_ALLOCATIONS 1000
#define MAX_ALLOCATION_SIZE 1024

// Thread test fonksiyonu
void* thread_test(void* arg) {
    void* ptrs[NUM_ALLOCATIONS];
    
    // Rastgele boyutlarda bellek ayır
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        ptrs[i] = mm_alloc(size);
        assert(ptrs[i] != NULL);
        
        // Belleği test et - yazma ve okuma
        memset(ptrs[i], i % 256, size);
    }
    
    // Bellekleri rastgele sırayla serbest bırak
    for (int i = NUM_ALLOCATIONS - 1; i >= 0; i--) {
        assert(mm_free(ptrs[i]) == MM_SUCCESS);
    }
    
    return NULL;
}

int main() {
    // Bellek yöneticisini başlat
    assert(mm_init() == MM_SUCCESS);
    printf("Bellek yöneticisi başlatıldı.\n");
    
    // Basit test
    printf("\n=== Basit Test ===\n");
    void* ptr1 = mm_alloc(128);
    void* ptr2 = mm_alloc(256);
    void* ptr3 = mm_alloc(512);
    
    assert(ptr1 && ptr2 && ptr3);
    printf("3 bellek bloğu başarıyla ayrıldı.\n");
    
    mm_print_stats();
    
    assert(mm_free(ptr2) == MM_SUCCESS);
    printf("Orta blok serbest bırakıldı.\n");
    
    void* ptr4 = mm_alloc(128);
    assert(ptr4);
    printf("Yeni blok ayrıldı.\n");
    
    mm_print_stats();
    
    assert(mm_free(ptr1) == MM_SUCCESS);
    assert(mm_free(ptr3) == MM_SUCCESS);
    assert(mm_free(ptr4) == MM_SUCCESS);
    
    // Multi-thread test
    printf("\n=== Multi-thread Test ===\n");
    pthread_t threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_test, NULL);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Multi-thread test tamamlandı.\n");
    mm_print_stats();
    
    // Bellek sızıntısı kontrolü
    bool has_leaks = mm_check_leaks();
    printf("Bellek sızıntısı kontrolü: %s\n", has_leaks ? "BAŞARISIZ" : "BAŞARILI");
    
    // Temizlik
    mm_cleanup();
    printf("Bellek yöneticisi temizlendi.\n");
    
    return 0;
} 