# Advanced Memory Management System

Bu proje, C dilinde geliştirilmiş ileri seviye bir bellek yönetim sistemidir.

## Özellikler
- Özel bellek havuzu yönetimi
- Bellek sızıntısı tespiti ve önleme
- Bellek fragmentasyonu optimizasyonu
- Thread-safe bellek yönetimi
- Detaylı bellek kullanım istatistikleri

## Derleme
```bash
gcc -o memory_manager src/*.c -Wall -Wextra -pthread
```

## Kullanım
```c
#include "memory_manager.h"

int main() {
    mm_init();  // Bellek yöneticisini başlat
    void* ptr = mm_alloc(1024);  // 1024 byte bellek ayır
    // ... belleği kullan ...
    mm_free(ptr);  // Belleği serbest bırak
    mm_cleanup();  // Bellek yöneticisini kapat
    return 0;
}
```