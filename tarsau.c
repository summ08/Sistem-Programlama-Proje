/*
 * tarsau.c
 * Basit .sau arşiv oluşturma ve açma programı
 *
 * Bu program iki temel işlem yapar:
 *
 * 1) Arşiv oluşturma (-b):
 *    Verilen metin dosyalarını tek bir .sau arşiv dosyası içinde birleştirir.
 *
 * 2) Arşiv açma (-a):
 *    Daha önce oluşturulmuş .sau arşiv dosyasını okuyup içindeki dosyaları
 *    tekrar diske çıkarır.
 *
 * Kullanım:
 *   tarsau -b dosya1 dosya2 ... [-o cikti.sau]
 *   tarsau -a arsiv.sau [dizin]
 *
 * Örnek:
 *   ./tarsau -b t1.txt t2.txt -o arsiv.sau
 *   ./tarsau -a arsiv.sau cikti
 */

#define _POSIX_C_SOURCE 200809L

/*
 * Standart C kütüphaneleri:
 * stdio.h   -> Dosya okuma/yazma ve ekrana yazdırma işlemleri için.
 * stdlib.h  -> malloc, free, atol, strtol gibi genel amaçlı fonksiyonlar için.
 * string.h  -> strlen, strcmp, strncpy, strchr, strrchr gibi string işlemleri için.
 * ctype.h   -> isdigit gibi karakter kontrol fonksiyonları için.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Sistem kütüphaneleri:
 * sys/stat.h  -> stat, chmod, mkdir ve dosya izinleri için.
 * sys/types.h -> mode_t gibi sistem tipleri için.
 * unistd.h    -> POSIX sistem çağrıları için.
 * errno.h     -> Hata durumunda sistem hata kodunu okumak için.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

 /*
  * İşletim sistemine göre klasör oluşturma komutunu ayarlar.
  *
  * Windows sistemlerde klasör oluşturmak için _mkdir() fonksiyonu kullanılır
  * ve bu fonksiyon <direct.h> başlık dosyasında tanımlıdır.
  *
  * Linux / macOS gibi POSIX tabanlı sistemlerde ise mkdir(path, 0755)
  * kullanılır. Buradaki 0755 değeri, oluşturulan klasörün izinlerini belirtir.
  *
  * Böylece kodun geri kalanında doğrudan MKDIR(path) yazılarak
  * hem Windows'ta hem de Linux/macOS'ta uyumlu şekilde klasör oluşturulabilir.
  */
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif

/*
 * Program sabitleri:
 *
 * MAKS_DOSYA:
 *   Bir arşive en fazla kaç dosya eklenebileceğini belirtir.
 *
 * MAKS_BOYUT:
 *   Arşive eklenecek dosyaların toplam boyut sınırıdır.
 *   Burada 200 MB olarak belirlenmiştir.
 *
 * VARSAYILAN_CIKTI:
 *   Kullanıcı -o parametresiyle çıktı adı vermezse oluşacak arşiv adıdır.
 *
 * BASLIK_UZUNLUK:
 *   .sau dosyasının en başındaki 10 baytlık alandır.
 *   Bu alan, bilgi bölümünün toplam boyutunu ASCII sayı olarak tutar.
 *
 * SAU_UZANTI:
 *   Arşiv açma sırasında dosyanın .sau uzantılı olup olmadığını kontrol etmek için kullanılır.
 */
#define MAKS_DOSYA       32
#define MAKS_BOYUT       (200L * 1024L * 1024L)
#define VARSAYILAN_CIKTI "a.sau"
#define BASLIK_UZUNLUK   10
#define SAU_UZANTI       ".sau"

/*
 * DosyaBilgi yapısı:
 *
 * Arşivden çıkarılacak her dosyanın bilgilerini bellekte tutmak için kullanılır.
 *
 * dosyaAdi:
 *   Arşiv içindeki dosyanın adıdır.
 *
 * izin:
 *   Dosyanın Linux/Unix izin bilgisidir.
 *   Örneğin 0644 gibi.
 *
 * boyut:
 *   Dosyanın byte cinsinden boyutudur.
 *   Arşiv açılırken kaç byte okunacağını belirlemek için kullanılır.
 */
typedef struct {
    char dosyaAdi[256];
    mode_t izin;
    long boyut;
} DosyaBilgi;


/*
 * sadeceDosyaAdi
 *
 * Amaç:
 *   Kullanıcı dosya yolunu klasör ile birlikte verirse, sadece dosya adını almak.
 *
 * Örnek:
 *   "klasor/t1.txt" verilirse "t1.txt" döner.
 *   "t1.txt" verilirse yine "t1.txt" döner.
 *
 * Not:
 *   Bu fonksiyon '/' karakterine göre ayırır.
 *   Linux/macOS yolları için uygundur.
 *   PowerShell'de de programı ./tarsau şeklinde çalıştırırken genellikle sorun çıkarmaz.
 */
static const char *sadeceDosyaAdi(const char *yol)
{
    /*
     * strrchr, verilen karakterin string içindeki son geçtiği yeri bulur.
     * Burada son '/' karakterini arıyoruz.
     */
    const char *p = strrchr(yol, '/');

    /*
     * Eğer '/' bulunduysa, dosya adı bu karakterden sonraki kısımdır.
     */
    if (p != NULL)
        return p + 1;

    /*
     * Eğer '/' yoksa kullanıcı zaten sadece dosya adı vermiştir.
     */
    return yol;
}


/*
 * metinDosyasiMi
 *
 * Amaç:
 *   Arşive eklenecek dosyanın ASCII metin dosyası olup olmadığını kontrol etmek.
 *
 * Çalışma mantığı:
 *   Dosya binary modda açılır.
 *   Dosyadaki bütün karakterler tek tek okunur.
 *   Eğer 127'den büyük ASCII değeri görülürse dosya uyumsuz kabul edilir.
 *
 * Dönüş değeri:
 *   1 -> Dosya ASCII metin dosyasıdır.
 *   0 -> Dosya açılamadı veya ASCII dışı karakter içeriyor.
 */
static int metinDosyasiMi(const char *yol)
{
    /*
     * Dosya binary modda açılır.
     * Böylece işletim sisteminin satır sonu dönüşümleri devreye girmez.
     */
    FILE *fp = fopen(yol, "rb");

    /*
     * Dosya açılamazsa uyumsuz kabul edilir.
     */
    if (fp == NULL)
        return 0;

    int karakter;

    /*
     * fgetc dosyadan bir karakter okur.
     * EOF dosyanın sonuna gelindiğini belirtir.
     */
    while ((karakter = fgetc(fp)) != EOF) {
        /*
         * ASCII karakterler 0-127 aralığındadır.
         * 127'den büyük değer varsa dosya saf ASCII değildir.
         */
        if ((unsigned char)karakter > 127) {
            fclose(fp);
            return 0;
        }
    }

    /*
     * Tüm karakterler uygunsa dosya kapatılır ve başarılı sonuç döndürülür.
     */
    fclose(fp);
    return 1;
}


/*
 * klasorOlustur
 *
 * Amaç:
 *   Arşiv açılırken hedef klasör yoksa oluşturmak.
 *
 * Özellik:
 *   Sadece tek bir klasörü değil, ara klasörleri de oluşturabilir.
 *
 * Örnek:
 *   "cikti/alt" verilirse önce "cikti", sonra "cikti/alt" oluşturulur.
 *
 * Dönüş değeri:
 *   0  -> Başarılı.
 *  -1  -> Hata oluştu.
 */
static int klasorOlustur(const char *yol)
{
    /*
     * Boş veya NULL klasör yolu geçersizdir.
     */
    if (yol == NULL || yol[0] == '\0')
        return -1;

    /*
     * Klasör yolu geçici bir diziye kopyalanır.
     * Çünkü yol üzerinde geçici olarak '\0' karakterleri yerleştireceğiz.
     */
    char gecici[4096];
    size_t uzunluk = strlen(yol);

    /*
     * Yol çok uzunsa tampon taşmasını önlemek için hata döndürülür.
     */
    if (uzunluk >= sizeof(gecici))
        return -1;

    strncpy(gecici, yol, sizeof(gecici) - 1);
    gecici[sizeof(gecici) - 1] = '\0';

    /*
     * Yolun sonunda '/' varsa kaldırılır.
     * Örnek: "cikti/" -> "cikti"
     */
    if (uzunluk > 1 && gecici[uzunluk - 1] == '/')
        gecici[uzunluk - 1] = '\0';

    /*
     * Yol içindeki her '/' karakterinde geçici olarak string sonlandırılır.
     * Böylece ara klasörler sırayla oluşturulur.
     */
    for (char *p = gecici + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            /*
             * mkdir klasörü oluşturur.
             * errno == EEXIST ise klasör zaten vardır, bu hata sayılmaz.
             */
            if (MKDIR(gecici) != 0 && errno != EEXIST) return -1;
                return -1;

            /*
             * Yol eski haline getirilir.
             */
            *p = '/';
        }
    }

    /*
     * En son asıl hedef klasör oluşturulur.
     */
    if (MKDIR(gecici) != 0 && errno != EEXIST) return -1;
        return -1;

    return 0;
}


/*
 * sauMu
 *
 * Amaç:
 *   Verilen dosya adının ".sau" uzantısıyla bitip bitmediğini kontrol etmek.
 *
 * Dönüş değeri:
 *   1 -> Dosya adı .sau ile bitiyor.
 *   0 -> Dosya adı .sau ile bitmiyor.
 */
static int sauMu(const char *dosyaAdi)
{
    size_t adUzunluk = strlen(dosyaAdi);
    size_t uzantiUzunluk = strlen(SAU_UZANTI);

    /*
     * Dosya adı uzantıdan kısa ise .sau ile bitmesi mümkün değildir.
     */
    if (adUzunluk < uzantiUzunluk)
        return 0;

    /*
     * Dosya adının son kısmı ".sau" ile aynı mı diye kontrol edilir.
     */
    return strcmp(dosyaAdi + adUzunluk - uzantiUzunluk, SAU_UZANTI) == 0;
}


/*
 * arsivOlustur
 *
 * Amaç:
 *   Komut satırından verilen dosyaları okuyup tek bir .sau arşivi oluşturmak.
 *
 * Kullanım:
 *   tarsau -b dosya1 dosya2 ... [-o cikti.sau]
 *
 * Örnek:
 *   ./tarsau -b t1.txt t2.txt -o arsiv.sau
 *
 * Arşiv formatı:
 *
 *   İlk 10 byte:
 *     Bilgi bölümünün toplam boyutu.
 *
 *   Bilgi bölümü:
 *     |dosyaAdi,izin,boyut|dosyaAdi,izin,boyut|
 *
 *   Veri bölümü:
 *     Dosyaların içerikleri ayraç olmadan peş peşe yazılır.
 */
static int arsivOlustur(int argc, char *argv[])
{
    /*
     * Kullanıcı -o vermezse çıktı dosyası varsayılan olarak a.sau olur.
     */
    const char *ciktiDosyasi = VARSAYILAN_CIKTI;

    /*
     * Arşive eklenecek dosyaların yolları bu dizide tutulur.
     */
    const char *dosyalar[MAKS_DOSYA];

    int dosyaSayisi = 0;

    /*
     * argv[0] program adı, argv[1] ise -b parametresidir.
     * Bu yüzden dosya adlarını okumaya argv[2]'den başlanır.
     */
    for (int i = 2; i < argc; i++) {
        /*
         * -o parametresi görülürse bir sonraki argüman çıktı dosyası adı kabul edilir.
         */
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Hata: -o parametresinden sonra dosya adı girilmedi!\n");
                return 1;
            }

            ciktiDosyasi = argv[++i];
        } else {
            /*
             * -o dışındaki argümanlar giriş dosyası kabul edilir.
             */
            if (dosyaSayisi >= MAKS_DOSYA) {
                fprintf(stderr, "Hata: En fazla %d dosya eklenebilir!\n", MAKS_DOSYA);
                return 1;
            }

            dosyalar[dosyaSayisi++] = argv[i];
        }
    }

    /*
     * Hiç dosya verilmemişse arşiv oluşturulamaz.
     */
    if (dosyaSayisi == 0) {
        fprintf(stderr, "Hata: Arşive eklenecek dosya girilmedi!\n");
        return 1;
    }

    /*
     * stat fonksiyonundan dönen dosya bilgileri bu değişkende tutulur.
     */
    struct stat dosyaDurumu;

    /*
     * Her dosyanın boyutu ve izin bilgisi ayrı dizilerde saklanır.
     * Daha sonra bilgi bölümü oluşturulurken bu bilgiler kullanılacaktır.
     */
    long dosyaBoyutlari[MAKS_DOSYA];
    mode_t dosyaIzinleri[MAKS_DOSYA];

    long toplamBoyut = 0;

    /*
     * Arşive eklenecek bütün dosyalar tek tek kontrol edilir.
     */
    for (int i = 0; i < dosyaSayisi; i++) {
        /*
         * stat başarısızsa dosya yoktur veya erişilememiştir.
         * S_ISREG kontrolü, verilen yolun normal dosya olup olmadığını kontrol eder.
         * Klasör gibi girişler kabul edilmez.
         */
        if (stat(dosyalar[i], &dosyaDurumu) != 0 || !S_ISREG(dosyaDurumu.st_mode)) {
            fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", dosyalar[i]);
            return 1;
        }

        /*
         * Ödev formatı ASCII metin dosyası istediği için dosyanın metin olup olmadığı kontrol edilir.
         */
        if (!metinDosyasiMi(dosyalar[i])) {
            fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", dosyalar[i]);
            return 1;
        }

        /*
         * Dosya boyutu ve izinleri ileride arşiv başlığına yazılmak üzere saklanır.
         */
        dosyaBoyutlari[i] = (long)dosyaDurumu.st_size;
        dosyaIzinleri[i] = dosyaDurumu.st_mode & 0777;

        /*
         * Toplam boyut 200 MB sınırını aşmasın diye biriktirilir.
         */
        toplamBoyut += dosyaBoyutlari[i];
    }

    /*
     * Toplam dosya boyutu sınırı aşarsa işlem iptal edilir.
     */
    if (toplamBoyut > MAKS_BOYUT) {
        fprintf(stderr, "Hata: Toplam dosya boyutu 200 MB sınırını aşıyor!\n");
        return 1;
    }

    /*
     * Bu bölümde her dosya için:
     *   dosya adı,
     *   izin bilgisi,
     *   dosya boyutu
     * tutulur.
     */
    char bilgiBolumu[65536];
    int bilgiUzunluk = 0;

    /*
     * Her dosya için bilgi bölümüne bir kayıt eklenir.
     */
    for (int i = 0; i < dosyaSayisi; i++) {
        /*
         * Dosya yolu verilmişse sadece dosya adı alınır.
         * Böylece arşivden çıkarırken klasör yolu değil, dosya adı kullanılır.
         */
        const char *ad = sadeceDosyaAdi(dosyalar[i]);

        /*
         * snprintf ile bilgi bölümünün sonuna yeni kayıt yazılır.
         *
         * Format:
         *   |dosyaAdi,izin,boyut
         *
         * Dikkat:
         *   Her kaydın başına '|' konuyor.
         *   En son ayrıca kapanış '|' karakteri eklenecek.
         */
        bilgiUzunluk += snprintf(
            bilgiBolumu + bilgiUzunluk,
            (int)sizeof(bilgiBolumu) - bilgiUzunluk,
            "|%s,%o,%ld",
            ad,
            (unsigned int)dosyaIzinleri[i],
            dosyaBoyutlari[i]
        );

        /*
         * Bilgi bölümü tamponu dolarsa arşiv oluşturma iptal edilir.
         */
        if (bilgiUzunluk >= (int)sizeof(bilgiBolumu) - 1) {
            fprintf(stderr, "Hata: Bilgi bölümü çok büyük!\n");
            return 1;
        }
    }

    /*
     * Bilgi bölümünün sonuna kapanış '|' karakteri eklenir.
     */
    bilgiBolumu[bilgiUzunluk++] = '|';
    bilgiBolumu[bilgiUzunluk] = '\0';

    /*
     * İlk 10 byte dahil bilgi bölümünün toplam uzunluğu hesaplanır.
     *
     * Ödev formatına göre ilk 10 byte, organizasyon bölümünün boyutunu belirtir.
     * Bu yüzden 10 byte başlık + bilgi bölümü uzunluğu birlikte yazılır.
     */
    long bilgiBolumuToplamBoyut = BASLIK_UZUNLUK + bilgiUzunluk;

    /*
     * Çıktı arşiv dosyası binary yazma modunda açılır.
     */
    FILE *arsiv = fopen(ciktiDosyasi, "wb");

    if (arsiv == NULL) {
        fprintf(stderr, "Hata: '%s' çıktı dosyası oluşturulamadı: %s\n",
                ciktiDosyasi, strerror(errno));
        return 1;
    }

    /*
     * İlk 10 byte'a bilgi bölümünün toplam boyutu yazılır.
     *
     * %010ld:
     *   Sayıyı 10 karakter genişliğinde yazar.
     *   Boş kalan yerleri 0 ile doldurur.
     *
     * Örnek:
     *   43 -> 0000000043
     */
    fprintf(arsiv, "%010ld", bilgiBolumuToplamBoyut);

    /*
     * Bilgi bölümü arşive yazılır.
     */
    if (fwrite(bilgiBolumu, 1, (size_t)bilgiUzunluk, arsiv) != (size_t)bilgiUzunluk) {
        fprintf(stderr, "Hata: Bilgi bölümü yazılamadı!\n");
        fclose(arsiv);

        /*
         * Hata oluştuğu için yarım kalmış arşiv dosyası silinir.
         */
        remove(ciktiDosyasi);
        return 1;
    }

    /*
     * Bilgi bölümü yazıldıktan sonra dosya içerikleri sırayla arşive eklenir.
     */
    for (int i = 0; i < dosyaSayisi; i++) {
        FILE *giris = fopen(dosyalar[i], "rb");

        if (giris == NULL) {
            fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", dosyalar[i]);
            fclose(arsiv);
            remove(ciktiDosyasi);
            return 1;
        }

        /*
         * Büyük dosyaları tek seferde belleğe almak yerine 8192 byte'lık parçalarla okuyoruz.
         */
        char tampon[8192];
        size_t okunan;

        /*
         * Dosya bitene kadar oku ve arşive yaz.
         */
        while ((okunan = fread(tampon, 1, sizeof(tampon), giris)) > 0) {
            if (fwrite(tampon, 1, okunan, arsiv) != okunan) {
                fprintf(stderr, "Hata: '%s' dosyası arşive yazılamadı!\n", dosyalar[i]);
                fclose(giris);
                fclose(arsiv);
                remove(ciktiDosyasi);
                return 1;
            }
        }

        fclose(giris);
    }

    /*
     * Tüm dosyalar başarıyla yazıldıysa arşiv kapatılır.
     */
    fclose(arsiv);

    printf("Dosyalar birleştirildi.\n");

    return 0;
}


/*
 * arsivAc
 *
 * Amaç:
 *   .sau arşiv dosyasını okuyup içindeki dosyaları tekrar çıkarmak.
 *
 * Kullanım:
 *   tarsau -a arsiv.sau [dizin]
 *
 * Örnek:
 *   ./tarsau -a arsiv.sau
 *   ./tarsau -a arsiv.sau cikti
 *
 * Çalışma sırası:
 *   1) Arşiv dosyasının .sau uzantısı kontrol edilir.
 *   2) İlk 10 byte okunur.
 *   3) Bilgi bölümü okunur ve dosya kayıtları ayrıştırılır.
 *   4) Hedef klasör varsa oluşturulur.
 *   5) Her dosya, bilgi bölümündeki boyut kadar okunup diske yazılır.
 */
static int arsivAc(int argc, char *argv[])
{
    /*
     * -a işleminde en az:
     *   argv[0] -> program adı
     *   argv[1] -> -a
     *   argv[2] -> arşiv dosyası
     * bulunmalıdır.
     */
    if (argc < 3) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        return 1;
    }

    /*
     * En fazla bir hedef klasör verilebilir.
     * Fazladan parametre varsa kullanım hatasıdır.
     */
    if (argc > 4) {
        fprintf(stderr, "Hata: -a parametresinden sonra en fazla 2 parametre alınabilir!\n");
        return 1;
    }

    const char *arsivDosyasi = argv[2];
    const char *hedefKlasor = NULL;

    /*
     * Kullanıcı hedef klasör verdiyse argv[3]'ten alınır.
     */
    if (argc >= 4)
        hedefKlasor = argv[3];

    /*
     * Arşiv dosyası .sau uzantılı değilse geçersiz kabul edilir.
     */
    if (!sauMu(arsivDosyasi)) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        return 1;
    }

    /*
     * Arşiv dosyası binary okuma modunda açılır.
     */
    FILE *arsiv = fopen(arsivDosyasi, "rb");

    if (arsiv == NULL) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        return 1;
    }

    /*
     * İlk 10 byte başlık alanıdır.
     * Bu alan bilgi bölümünün toplam boyutunu ASCII sayı olarak tutar.
     */
    char baslik[BASLIK_UZUNLUK + 1];

    if (fread(baslik, 1, BASLIK_UZUNLUK, arsiv) != (size_t)BASLIK_UZUNLUK) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(arsiv);
        return 1;
    }

    /*
     * String işlemleri yapabilmek için sonuna '\0' eklenir.
     */
    baslik[BASLIK_UZUNLUK] = '\0';

    /*
     * Başlıktaki 10 karakterin tamamı rakam olmalıdır.
     * Aksi halde arşiv formatı bozuk kabul edilir.
     */
    for (int i = 0; i < BASLIK_UZUNLUK; i++) {
        if (!isdigit((unsigned char)baslik[i])) {
            fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
            fclose(arsiv);
            return 1;
        }
    }

    /*
     * Başlıktaki ASCII sayı long türüne çevrilir.
     */
    long bilgiBolumuToplamBoyut = atol(baslik);

    /*
     * Toplam bilgi bölümü boyutundan 10 byte başlık çıkarılır.
     * Geriye sadece kayıtların bulunduğu bilgi bölümü kalır.
     */
    long bilgiUzunluk = bilgiBolumuToplamBoyut - BASLIK_UZUNLUK;

    if (bilgiUzunluk <= 0) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(arsiv);
        return 1;
    }

    /*
     * Bilgi bölümü uzunluğu arşive göre değiştiği için dinamik bellek ayrılır.
     */
    char *bilgiBolumu = malloc((size_t)bilgiUzunluk + 1);

    if (bilgiBolumu == NULL) {
        fprintf(stderr, "Hata: Bellek ayrılamadı!\n");
        fclose(arsiv);
        return 1;
    }

    /*
     * Bilgi bölümü arşivden okunur.
     */
    if (fread(bilgiBolumu, 1, (size_t)bilgiUzunluk, arsiv) != (size_t)bilgiUzunluk) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        free(bilgiBolumu);
        fclose(arsiv);
        return 1;
    }

    bilgiBolumu[bilgiUzunluk] = '\0';

    /*
     * Bilgi bölümünden çıkarılan dosya kayıtları bu dizide tutulur.
     */
    DosyaBilgi kayitlar[MAKS_DOSYA];
    int kayitSayisi = 0;

    /*
     * p işaretçisi bilgi bölümü üzerinde dolaşmak için kullanılır.
     */
    char *p = bilgiBolumu;

    /*
     * Bilgi bölümü şu yapıda beklenir:
     *
     *   |dosyaAdi,izin,boyut|dosyaAdi,izin,boyut|
     *
     * Her kayıt '|' karakterleri arasındadır.
     */
    while (*p == '|' && kayitSayisi < MAKS_DOSYA) {
        /*
         * Baştaki '|' karakteri geçilir.
         */
        p++;

        /*
         * Sonraki '|' karakteri kaydın sonudur.
         */
        char *son = strchr(p, '|');

        if (son == NULL)
            break;

        /*
         * Kayıt sonunu geçici olarak string sonu yapıyoruz.
         * Böylece bu kayıt üzerinde rahatça strchr ile işlem yapılabilir.
         */
        *son = '\0';

        /*
         * Kayıt alanları:
         *   adAlani,izinAlani,boyutAlani
         */
        char *adAlani = p;
        char *virgul1 = strchr(adAlani, ',');

        /*
         * İlk virgül yoksa kayıt bozuk kabul edilir ve sonraki kayda geçilir.
         */
        if (virgul1 == NULL) {
            p = son + 1;
            continue;
        }

        /*
         * İlk virgül string sonu yapılır.
         * Böylece adAlani yalnızca dosya adını gösterir.
         */
        *virgul1 = '\0';

        char *izinAlani = virgul1 + 1;
        char *virgul2 = strchr(izinAlani, ',');

        /*
         * İkinci virgül yoksa kayıt bozuk kabul edilir.
         */
        if (virgul2 == NULL) {
            p = son + 1;
            continue;
        }

        /*
         * İkinci virgül de string sonu yapılır.
         * Böylece izinAlani yalnızca izin bilgisini gösterir.
         */
        *virgul2 = '\0';

        /*
         * İkinci virgülden sonraki kısım boyut alanıdır.
         */
        char *boyutAlani = virgul2 + 1;

        /*
         * Dosya adı güvenli şekilde kayıt dizisine kopyalanır.
         */
        strncpy(kayitlar[kayitSayisi].dosyaAdi, adAlani, 255);
        kayitlar[kayitSayisi].dosyaAdi[255] = '\0';

        /*
         * İzin bilgisi sekizlik tabanda okunur.
         * Çünkü izinler 644, 755 gibi octal ifade edilir.
         */
        kayitlar[kayitSayisi].izin = (mode_t)strtol(izinAlani, NULL, 8);

        /*
         * Boyut bilgisi decimal sayı olarak okunur.
         */
        kayitlar[kayitSayisi].boyut = atol(boyutAlani);

        kayitSayisi++;

        /*
         * Bir sonraki kayda geçilir.
         */
        p = son + 1;
    }

    /*
     * Bilgi bölümü artık ayrıştırıldığı için ayrılan bellek serbest bırakılır.
     */
    free(bilgiBolumu);

    /*
     * Hiç geçerli kayıt bulunamadıysa arşiv bozuk kabul edilir.
     */
    if (kayitSayisi == 0) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(arsiv);
        return 1;
    }

    /*
     * Kullanıcı hedef klasör verdiyse klasörün varlığı kontrol edilir.
     */
    if (hedefKlasor != NULL) {
        struct stat klasorDurumu;

        /*
         * stat başarısızsa klasör yoktur.
         * Bu durumda klasör oluşturulmaya çalışılır.
         */
        if (stat(hedefKlasor, &klasorDurumu) != 0) {
            if (klasorOlustur(hedefKlasor) != 0) {
                fprintf(stderr, "Hata: '%s' dizini oluşturulamadı: %s\n",
                        hedefKlasor, strerror(errno));
                fclose(arsiv);
                return 1;
            }
        } else if (!S_ISDIR(klasorDurumu.st_mode)) {
            /*
             * Aynı isimde bir dosya varsa klasör oluşturulamaz.
             */
            fprintf(stderr, "Hata: '%s' zaten dosya olarak mevcut!\n", hedefKlasor);
            fclose(arsiv);
            return 1;
        }
    }

    /*
     * Arşivdeki her kayıt için ilgili dosya içerikten okunup diske yazılır.
     */
    for (int i = 0; i < kayitSayisi; i++) {
        char cikacakDosya[4096];

        /*
         * Hedef klasör verilmişse dosya o klasörün içine çıkarılır.
         * Verilmemişse mevcut çalışma dizinine çıkarılır.
         */
        if (hedefKlasor != NULL) {
            snprintf(cikacakDosya, sizeof(cikacakDosya),
                     "%s/%s", hedefKlasor, kayitlar[i].dosyaAdi);
        } else {
            snprintf(cikacakDosya, sizeof(cikacakDosya),
                     "%s", kayitlar[i].dosyaAdi);
        }

        /*
         * Çıkarılacak dosya binary yazma modunda oluşturulur.
         */
        FILE *cikis = fopen(cikacakDosya, "wb");

        if (cikis == NULL) {
            fprintf(stderr, "Hata: '%s' dosyası oluşturulamadı: %s\n",
                    cikacakDosya, strerror(errno));
            fclose(arsiv);
            return 1;
        }

        /*
         * Bu dosya için arşivden okunması gereken byte sayısı.
         */
        long kalan = kayitlar[i].boyut;

        /*
         * Okuma/yazma işlemi yine parça parça yapılır.
         */
        char tampon[8192];

        /*
         * Yazma hatası olup olmadığını takip eder.
         */
        int hataVar = 0;

        /*
         * Kayıtta belirtilen boyut kadar veri arşivden okunur.
         */
        while (kalan > 0) {
            size_t okunacak;

            /*
             * Eğer kalan veri tampon boyutundan küçükse sadece kalan kadar okunur.
             */
            if (kalan < (long)sizeof(tampon))
                okunacak = (size_t)kalan;
            else
                okunacak = sizeof(tampon);

            /*
             * Arşivden veri okunur.
             */
            size_t okunan = fread(tampon, 1, okunacak, arsiv);

            /*
             * Okuma yapılamadıysa dosya beklenenden kısa olabilir.
             */
            if (okunan == 0)
                break;

            /*
             * Okunan veri çıkış dosyasına yazılır.
             */
            if (fwrite(tampon, 1, okunan, cikis) != okunan) {
                hataVar = 1;
                break;
            }

            /*
             * Başarıyla yazılan byte kadar kalan miktar azaltılır.
             */
            kalan -= (long)okunan;
        }

        fclose(cikis);

        /*
         * Hata varsa veya beklenen tüm veri okunamadıysa işlem başarısızdır.
         */
        if (hataVar || kalan != 0) {
            fprintf(stderr, "Hata: '%s' dosyası tam yazılamadı!\n", cikacakDosya);
            fclose(arsiv);
            return 1;
        }

        /*
         * Dosyanın izinleri arşivde saklanan izin değerine göre ayarlanır.
         */
        chmod(cikacakDosya, kayitlar[i].izin);
    }

    /*
     * Tüm dosyalar çıkarıldıktan sonra arşiv kapatılır.
     */
    fclose(arsiv);

    /*
     * Kullanıcıya hangi dosyaların açıldığı yazdırılır.
     */
    if (hedefKlasor != NULL) {
        printf("%s dizininde ", hedefKlasor);
    }

    for (int i = 0; i < kayitSayisi; i++) {
        if (i > 0)
            printf(", ");

        printf("%s", kayitlar[i].dosyaAdi);
    }

    printf(" dosyalar%s acildi.\n");

    return 0;
}


/*
 * main
 *
 * Programın başlangıç noktasıdır.
 *
 * Görevi:
 *   Komut satırı parametrelerine bakarak hangi işlemin yapılacağını seçmek.
 *
 * Desteklenen işlemler:
 *   -b -> Arşiv oluşturma
 *   -a -> Arşiv açma
 */
int main(int argc, char *argv[])
{
    /*
     * En az bir işlem parametresi verilmelidir.
     * Örneğin -b veya -a.
     */
    if (argc < 2) {
        fprintf(stderr,
                "Kullanım:\n"
                "  tarsau -b dosya1 dosya2 ... [-o cikti.sau]\n"
                "  tarsau -a arsiv.sau [dizin]\n");
        return 1;
    }

    /*
     * İlk parametre -b ise arşiv oluşturma fonksiyonu çağrılır.
     */
    if (strcmp(argv[1], "-b") == 0) {
        return arsivOlustur(argc, argv);
    }

    /*
     * İlk parametre -a ise arşiv açma fonksiyonu çağrılır.
     */
    if (strcmp(argv[1], "-a") == 0) {
        return arsivAc(argc, argv);
    }

    /*
     * -b veya -a dışında bir parametre verilirse hata mesajı gösterilir.
     */
    fprintf(stderr, "Hata: Geçersiz parametre '%s'.\n", argv[1]);
    fprintf(stderr,
            "Kullanım:\n"
            "  tarsau -b dosya1 dosya2 ... [-o cikti.sau]\n"
            "  tarsau -a arsiv.sau [dizin]\n");

    return 1;
}
