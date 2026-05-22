#include "tarsau.h"
#include <direct.h>
#ifdef _WIN32



static void print_hata(const char *filename) {
   fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", filename);
}

  
