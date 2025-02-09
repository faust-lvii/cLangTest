#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_THREADS 2
#define NUM_ALLOCATIONS 500
#define MAX_ALLOCATION_SIZE 512

// Thread test fonksiyonu
DWORD WINAPI thread_test(LPVOID arg) {
    void* ptrs[NUM_ALLOCATIONS];
    
    // Rastgele boyutlarda bellek ayır
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        ptrs[i] = mm_alloc(size);
        if (!ptrs[i]) {
            printf("Bellek ayırma hatası!\n");
            return 1;
        }
        
        // Belleği test et - yazma ve okuma
        memset(ptrs[i], i % 256, size);
    }
    
    // Bellekleri rastgele sırayla serbest bırak
    for (int i = NUM_ALLOCATIONS - 1; i >= 0; i--) {
        if (mm_free(ptrs[i]) != MM_SUCCESS) {
            printf("Bellek serbest bırakma hatası!\n");
            return 1;
        }
    }
    
    return 0;
}

int main() {
    srand((unsigned int)time(NULL));

    // Bellek yöneticisini başlat
    if (mm_init() != MM_SUCCESS) {
        printf("Bellek yöneticisi başlatılamadı!\n");
        return 1;
    }
    printf("Bellek yöneticisi başlatıldı.\n");
    
    // Basit test
    printf("\n=== Basit Test ===\n");
    void* ptr1 = mm_alloc(128);
    void* ptr2 = mm_alloc(256);
    void* ptr3 = mm_alloc(512);
    
    if (!ptr1 || !ptr2 || !ptr3) {
        printf("Bellek ayırma hatası!\n");
        return 1;
    }
    printf("3 bellek bloğu başarıyla ayrıldı.\n");
    
    mm_print_stats();
    
    if (mm_free(ptr2) != MM_SUCCESS) {
        printf("Bellek serbest bırakma hatası!\n");
        return 1;
    }
    printf("Orta blok serbest bırakıldı.\n");
    
    void* ptr4 = mm_alloc(128);
    if (!ptr4) {
        printf("Bellek ayırma hatası!\n");
        return 1;
    }
    printf("Yeni blok ayrıldı.\n");
    
    mm_print_stats();
    
    if (mm_free(ptr1) != MM_SUCCESS ||
        mm_free(ptr3) != MM_SUCCESS ||
        mm_free(ptr4) != MM_SUCCESS) {
        printf("Bellek serbest bırakma hatası!\n");
        return 1;
    }
    
    // Multi-thread test
    printf("\n=== Multi-thread Test ===\n");
    HANDLE threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = CreateThread(NULL, 0, thread_test, NULL, 0, NULL);
        if (threads[i] == NULL) {
            printf("Thread oluşturma hatası!\n");
            return 1;
        }
    }
    
    // Tüm thread'lerin tamamlanmasını bekle
    WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);
    
    // Thread handle'larını kapat
    for (int i = 0; i < NUM_THREADS; i++) {
        CloseHandle(threads[i]);
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