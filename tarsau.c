#include "tarsau.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

static void print_hata(const char *filename) {
    fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", filename);
}

// Dosyanın metin dosyası olup olmadığını kontrol eder
int txt_mi(const char *filename) {
    FILE *dosya = fopen(filename, "rb");
    if (!dosya) {
        return 0;
    }
    
    // İlk 512 byte'ı kontrol et
    unsigned char gecici_buffer[512];
    size_t okunan_bayt = fread(gecici_buffer, 1, sizeof(gecici_buffer), dosya);
    fclose(dosya);
    
    // NULL karakter veya ASCII dışı karakterler varsa metin değildir
    for (size_t i = 0; i < okunan_bayt; i++) {
        // ASCII yazdırılabilir karakterler, tab, newline, carriage return
        if (gecici_buffer[i] == 0) {
            return 0;
        }
        if (gecici_buffer[i] > 127) {
            // UTF-8 veya diğer encoding'ler olabilir, ama proje ASCII istiyor
            return 0;
        }
    }
    
    return 1;
}

// Dizini özyinelemeli oluşturur
void dizin_olustur(const char *path) {
    char tmp[MAX_PATH_LENGTH];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// Arşiv dosyasını doğrular
int arsiv_dogrula(const char *filename) {
    FILE *dosya = fopen(filename, "rb");
    if (!dosya) {
        return 0;
    }
    
    // .sau uzantısını kontrol et
    const char *ext = strrchr(filename, '.');
    if (!ext || strcmp(ext, ".sau") != 0) {
        fclose(dosya);
        return 0;
    }
    
    // İlk 10 byte'ı oku (header size)
    char baslik_boyutu_metni[HEADER_SIZE_LENGTH + 1];
    size_t okunma_sayisi = fread(baslik_boyutu_metni, 1, HEADER_SIZE_LENGTH, dosya);
    if (okunma_sayisi != HEADER_SIZE_LENGTH) {
        fclose(dosya);
        return 0;
    }
    
    fclose(dosya);
    return 1;
}

// Arşiv oluşturur (-b parametresi)
int arsiv_olustur(char **giris_dosyalar, int dosya_sayisi, const char *cikti_dosyasi) {
    Archive arsiv = {0};
    FILE *arsiv_cikti = NULL;
    long toplam_veri_boyutu = 0;
    
    // Dosya sayısı kontrolü
    if (dosya_sayisi > MAX_FILES) {
        fprintf(stderr, "Hata: En fazla %d dosya arsivlenebilir!\n", MAX_FILES);
        return 1;
    }
    
    // Her dosyayı kontrol et ve bilgilerini topla
    for (int i = 0; i < dosya_sayisi; i++) {
        struct stat st;
        
        // Dosya var mı?
        if (stat(giris_dosyalar[i], &st) != 0) {
            fprintf(stderr, "Hata: '%s' dosyasi bulunamadi!\n", giris_dosyalar[i]);
            return 1;
        }
        
        // Normal dosya mı?
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Hata: '%s' normal bir dosya degil!\n", giris_dosyalar[i]);
            return 1;
        }
        
        // Metin dosyası mı?
        if (!txt_mi(giris_dosyalar[i])) {
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", giris_dosyalar[i]);
            return 1;
        }
        
        // Dosya bilgilerini kaydet
        strncpy(arsiv.files[i].name, giris_dosyalar[i], MAX_PATH_LENGTH - 1);
        arsiv.files[i].permissions = st.st_mode;
        arsiv.files[i].size = st.st_size;
        
        toplam_veri_boyutu += st.st_size;
    }
    
    arsiv.file_count = dosya_sayisi;
    arsiv.total_size = toplam_veri_boyutu;
    
    // Toplam boyut kontrolü
    if (toplam_veri_boyutu > MAX_FILE_SIZE) {
        fprintf(stderr, "Hata: Toplam dosya boyutu 200 MB'i asiyor!\n");
        return 1;
    }
    
    // Organizasyon bilgilerini hazırla
    long arsiv_bilgi_boyutu = 0;
    for (int i = 0; i < arsiv.file_count; i++) {
        int entry_len = snprintf(NULL, 0, "|%s,%o,%ld",
                                 arsiv.files[i].name,
                                 arsiv.files[i].permissions & 0777,
                                 arsiv.files[i].size);
        if (entry_len < 0) {
            fprintf(stderr, "Hata: Organizasyon bilgileri olusturulamadi!\n");
            return 1;
        }
        arsiv_bilgi_boyutu += entry_len;
    }

    if (arsiv_bilgi_boyutu <= 0 || arsiv_bilgi_boyutu > MAX_FILE_SIZE) {
        fprintf(stderr, "Hata: Organizasyon bilgileri cok buyuk!\n");
        return 1;
    }

    char *arsiv_bilgileri = malloc((size_t)arsiv_bilgi_boyutu + 1);
    if (!arsiv_bilgileri) {
        fprintf(stderr, "Bellek hatasi!\n");
        return 1;
    }

    size_t arsiv_bilgi_ofseti = 0;
    for (int i = 0; i < arsiv.file_count; i++) {
        arsiv_bilgi_ofseti += (size_t)sprintf(arsiv_bilgileri + arsiv_bilgi_ofseti, "|%s,%o,%ld",
                                              arsiv.files[i].name,
                                              arsiv.files[i].permissions & 0777,
                                              arsiv.files[i].size);
    }

    // Çıktı dosyasını oluştur
    arsiv_cikti = fopen(cikti_dosyasi, "wb");
    if (!arsiv_cikti) {
        fprintf(stderr, "Hata: '%s' dosyasi olusturulamadi!\n", cikti_dosyasi);
        free(arsiv_bilgileri);
        return 1;
    }

    // 1. Header: İlk 10 byte organizasyon boyutu
    fprintf(arsiv_cikti, "%010ld", arsiv_bilgi_boyutu);

    // 2. Organizasyon bilgileri
    fwrite(arsiv_bilgileri, 1, (size_t)arsiv_bilgi_boyutu, arsiv_cikti);

    // 3. Dosya içeriklerini ekle
    for (int i = 0; i < arsiv.file_count; i++) {
        FILE *arsiv_giris = fopen(arsiv.files[i].name, "rb");
        if (!arsiv_giris) {
            fprintf(stderr, "Hata: '%s' dosyasi okunamadi!\n", arsiv.files[i].name);
            fclose(arsiv_cikti);
            free(arsiv_bilgileri);
            return 1;
        }
        
        // Dosya içeriğini kopyala
        char gecici_buffer[4096];
        size_t okunan_bayt;
        while ((okunan_bayt = fread(gecici_buffer, 1, sizeof(gecici_buffer), arsiv_giris)) > 0) {
            fwrite(gecici_buffer, 1, okunan_bayt, arsiv_cikti);
        }
        
        fclose(arsiv_giris);
    }

    fclose(arsiv_cikti);
    free(arsiv_bilgileri);
    printf("Dosyalar birlestirildi.\n");
    return 0;
}

// Arşivi çıkartır (-a parametresi)
int arsiv_ac(const char *arsiv_dosyasi, const char *cikti_klasoru) {
    FILE *arsiv_giris = NULL;
    Archive arsiv = {0};
    char baslik_boyutu_metni[HEADER_SIZE_LENGTH + 1];
    long arsiv_bilgi_boyutu;
    char *arsiv_bilgileri = NULL;
    
    // Arşiv dosyasını doğrula
    if (!arsiv_dogrula(arsiv_dosyasi)) {
        print_hata(arsiv_dosyasi);
        return 1;
    }
    
    arsiv_giris = fopen(arsiv_dosyasi, "rb");
    if (!arsiv_giris) {
        print_hata(arsiv_dosyasi);
        return 1;
    }
    
    // 1. Header'ı oku (organizasyon boyutu)
    if (fread(baslik_boyutu_metni, 1, HEADER_SIZE_LENGTH, arsiv_giris) != HEADER_SIZE_LENGTH) {
        print_hata(arsiv_dosyasi);
        fclose(arsiv_giris);
        return 1;
    }
    baslik_boyutu_metni[HEADER_SIZE_LENGTH] = '\0';
    arsiv_bilgi_boyutu = atol(baslik_boyutu_metni);
    
    if (arsiv_bilgi_boyutu <= 0 || arsiv_bilgi_boyutu > MAX_FILE_SIZE) {
        print_hata(arsiv_dosyasi);
        fclose(arsiv_giris);
        return 1;
    }
    
    // 2. Organizasyon bilgilerini oku
    arsiv_bilgileri = malloc(arsiv_bilgi_boyutu + 1);
    if (!arsiv_bilgileri) {
        fprintf(stderr, "Bellek hatasi!\n");
        fclose(arsiv_giris);
        return 1;
    }
    
    if (fread(arsiv_bilgileri, 1, arsiv_bilgi_boyutu, arsiv_giris) != (size_t)arsiv_bilgi_boyutu) {
        print_hata(arsiv_dosyasi);
        free(arsiv_bilgileri);
        fclose(arsiv_giris);
        return 1;
    }
    arsiv_bilgileri[arsiv_bilgi_boyutu] = '\0';
    
    // 3. Organizasyon bilgilerini parse et
    char *parca = strtok(arsiv_bilgileri, "|");
    int dosya_indeksi = 0;
    
    while (parca != NULL && dosya_indeksi < MAX_FILES) {
        char dosya_adi[MAX_PATH_LENGTH];
        unsigned int izinler;
        long boyut;
        
        if (sscanf(parca, "%[^,],%o,%ld", dosya_adi, &izinler, &boyut) == 3) {
            strncpy(arsiv.files[dosya_indeksi].name, dosya_adi, MAX_PATH_LENGTH - 1);
            arsiv.files[dosya_indeksi].permissions = izinler;
            arsiv.files[dosya_indeksi].size = boyut;
            dosya_indeksi++;
        }
        
        parca = strtok(NULL, "|");
    }
    
    arsiv.file_count = dosya_indeksi;
    free(arsiv_bilgileri);
    
    // 4. Çıktı dizinini oluştur
    if (cikti_klasoru && strlen(cikti_klasoru) > 0) {
        dizin_olustur(cikti_klasoru);
    }
    
    // 5. Dosyaları çıkart
    for (int i = 0; i < arsiv.file_count; i++) {
        char cikti_yolu[MAX_PATH_LENGTH];
        FILE *arsiv_cikti = NULL;
        
        // Çıktı yolunu oluştur
        if (cikti_klasoru && strlen(cikti_klasoru) > 0) {
            snprintf(cikti_yolu, sizeof(cikti_yolu), "%s/%s", 
                    cikti_klasoru, arsiv.files[i].name);
        } else {
            snprintf(cikti_yolu, sizeof(cikti_yolu), "%s", 
                    arsiv.files[i].name);
        }
        
        // Dosyayı oluştur
        arsiv_cikti = fopen(cikti_yolu, "wb");
        if (!arsiv_cikti) {
            fprintf(stderr, "Hata: '%s' dosyasi olusturulamadi!\n", cikti_yolu);
            fclose(arsiv_giris);
            return 1;
        }
        
        // Dosya içeriğini kopyala
        char gecici_buffer[4096];
        size_t kalan_boyut = (size_t)arsiv.files[i].size;
        
        while (kalan_boyut > 0) {
            size_t okunacak_bayt = (kalan_boyut < sizeof(gecici_buffer)) ? kalan_boyut : sizeof(gecici_buffer);
            size_t okunan_bayt = fread(gecici_buffer, 1, okunacak_bayt, arsiv_giris);
            
            if (okunan_bayt == 0) {
                print_hata(arsiv_dosyasi);
                fclose(arsiv_giris);
                return 1;
            }
            
            fwrite(gecici_buffer, 1, okunan_bayt, arsiv_cikti);
            kalan_boyut -= okunan_bayt;
        }
        
        fclose(arsiv_cikti);
        
        // İzinleri ayarla
#ifdef _WIN32
        _chmod(cikti_yolu, arsiv.files[i].permissions);
#else
        chmod(cikti_yolu, arsiv.files[i].permissions);
#endif
    }
    
    fclose(arsiv_giris);
    
    // Sonucu yazdır
    if (cikti_klasoru && strlen(cikti_klasoru) > 0) {
        printf("%s dizininde ", cikti_klasoru);
    }
    
    for (int i = 0; i < arsiv.file_count; i++) {
        printf("%s", arsiv.files[i].name);
        if (i < arsiv.file_count - 1) {
            printf(", ");
        }
    }
    printf(" dosyalar%s acildi.\n", arsiv.file_count > 1 ? "i" : "");
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Hata: Gecersiz parametre '%s'\n", argc > 1 ? argv[1] : "");
        return 1;
    }
    
    // -b parametresi: Arşiv oluştur
    if (strcmp(argv[1], "-b") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Hata: En az bir giris dosyasi gerekli!\n");
            return 1;
        }
        
        char *giris_dosyalar[MAX_FILES];
        int dosya_sayisi = 0;
        char *cikti_dosyasi = DEFAULT_ARCHIVE;
        
        // Parametreleri parse et
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc) {
                    cikti_dosyasi = argv[i + 1];
                    i++;
                } else {
                    fprintf(stderr, "Hata: -o parametresi icin deger veriniz!\n");
                    return 1;
                }
            } else {
                if (dosya_sayisi >= MAX_FILES) {
                    fprintf(stderr, "Hata: En fazla %d dosya arsivlenebilir!\n", MAX_FILES);
                    return 1;
                }
                giris_dosyalar[dosya_sayisi++] = argv[i];
            }
        }
        
        if (dosya_sayisi == 0) {
            fprintf(stderr, "Hata: En az bir giris dosyasi gerekli!\n");
            return 1;
        }
        
        return arsiv_olustur(giris_dosyalar, dosya_sayisi, cikti_dosyasi);
    }
    
    // -a parametresi: Arşivi çıkart
    else if (strcmp(argv[1], "-a") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Hata: Arsiv dosyasi gerekli!\n");
            return 1;
        }
        
        if (argc > 4) {
            fprintf(stderr, "Hata: -a parametresi en fazla 2 parametre alir!\n");
            return 1;
        }
        
        char *arsiv_dosyasi = argv[2];
        char *cikti_klasoru = (argc == 4) ? argv[3] : NULL;
        
        return arsiv_ac(arsiv_dosyasi, cikti_klasoru);
    }
    
    // Geçersiz parametre
    else {
        fprintf(stderr, "Hata: Gecersiz parametre '%s'\n", argv[1]);
        return 1;
    }
    
    return 0;
}
