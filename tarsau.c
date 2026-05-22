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


int arsiv_dogrula(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }
    
    // .sau uzantısını kontrol et
    const char *ext = strrchr(filename, '.');
    if (!ext || strcmp(ext, ".sau") != 0) {
        fclose(file);
        return 0;
    }
    
    // İlk 10 byte'ı oku (header size)
    char header_size_str[HEADER_SIZE_LENGTH + 1];
    size_t read = fread(header_size_str, 1, HEADER_SIZE_LENGTH, file);
    if (read != HEADER_SIZE_LENGTH) {
        fclose(file);
        return 0;
    }
    
    fclose(file);
    return 1;
}
// Arşiv oluşturur (-b parametresi)
int arsiv_olustur(char **input_files, int file_count, const char *output_file) {
    Archive archive = {0};
    FILE *out = NULL;
    long total_data_size = 0;
    
    // Dosya sayısı kontrolü
    if (file_count > MAX_FILES) {
        fprintf(stderr, "Hata: En fazla %d dosya arsivlenebilir!\n", MAX_FILES);
        return 1;
    }
    
    // Her dosyayı kontrol et ve bilgilerini topla
    for (int i = 0; i < file_count; i++) {
        struct stat st;
        
        // Dosya var mı?
        if (stat(input_files[i], &st) != 0) {
            fprintf(stderr, "Hata: '%s' dosyasi bulunamadi!\n", input_files[i]);
            return 1;
        }
        
        // Normal dosya mı?
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Hata: '%s' normal bir dosya degil!\n", input_files[i]);
            return 1;
        }
        
        // Metin dosyası mı?
        if (!txt_mi(input_files[i])) {
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", input_files[i]);
            return 1;
        }
        
        // Dosya bilgilerini kaydet
        strncpy(archive.files[i].name, input_files[i], MAX_PATH_LENGTH - 1);
        archive.files[i].permissions = st.st_mode;
        archive.files[i].size = st.st_size;
        
        total_data_size += st.st_size;
    }
    
    archive.file_count = file_count;
    archive.total_size = total_data_size;
    
    // Toplam boyut kontrolü
    if (total_data_size > MAX_FILE_SIZE) {
        fprintf(stderr, "Hata: Toplam dosya boyutu 200 MB'i asiyor!\n");
        return 1;
    }
    
    // Organizasyon bilgilerini hazırla
    long org_size = 0;
    for (int i = 0; i < archive.file_count; i++) {
        int entry_len = snprintf(NULL, 0, "|%s,%o,%ld",
                                 archive.files[i].name,
                                 archive.files[i].permissions & 0777,
                                 archive.files[i].size);
        if (entry_len < 0) {
            fprintf(stderr, "Hata: Organizasyon bilgileri olusturulamadi!\n");
            return 1;
        }
        org_size += entry_len;
    }

    if (org_size <= 0 || org_size > MAX_FILE_SIZE) {
        fprintf(stderr, "Hata: Organizasyon bilgileri cok buyuk!\n");
        return 1;
    }

    char *organization = malloc((size_t)org_size + 1);
    if (!organization) {
        fprintf(stderr, "Bellek hatasi!\n");
        return 1;
    }

    size_t org_offset = 0;
    for (int i = 0; i < archive.file_count; i++) {
        org_offset += (size_t)sprintf(organization + org_offset, "|%s,%o,%ld",
                                      archive.files[i].name,
                                      archive.files[i].permissions & 0777,
                                      archive.files[i].size);
    }

    // Çıktı dosyasını oluştur
    out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Hata: '%s' dosyasi olusturulamadi!\n", output_file);
        free(organization);
        return 1;
    }

    // 1. Header: İlk 10 byte organizasyon boyutu
    fprintf(out, "%010ld", org_size);

    // 2. Organizasyon bilgileri
    fwrite(organization, 1, (size_t)org_size, out);

    // 3. Dosya içeriklerini ekle
    for (int i = 0; i < archive.file_count; i++) {
        FILE *in = fopen(archive.files[i].name, "rb");
        if (!in) {
            fprintf(stderr, "Hata: '%s' dosyasi okunamadi!\n", archive.files[i].name);
            fclose(out);
            free(organization);
            return 1;
        }
        
        // Dosya içeriğini kopyala
        char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            fwrite(buffer, 1, bytes, out);
        }
        
        fclose(in);
    }

    fclose(out);
    free(organization);
    printf("Dosyalar birlestirildi.\n");
    return 0;
}  
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

