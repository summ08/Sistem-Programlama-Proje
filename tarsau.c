#include "tarsau.h"
#include <direct.h>
#ifdef _WIN32



static void print_hata(const char *filename) {
   fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", filename);
}

// Dosyanın metin dosyası olup olmadığını kontrol eder
int txt_mi(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }
    
    // İlk 512 byte'ı kontrol et
    unsigned char buffer[512];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);
    
    // NULL karakter veya ASCII dışı karakterler varsa metin değildir
    for (size_t i = 0; i < bytes_read; i++) {
        // ASCII yazdırılabilir karakterler, tab, newline, carriage return
        if (buffer[i] == 0) {
            return 0;
        }
        if (buffer[i] > 127) {
            // UTF-8 veya diğer encoding'ler olabilir, ama proje ASCII istiyor
            return 0;
        }
    }
    
    return 1;
}
  
