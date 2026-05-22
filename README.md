

BİLGİSAYAR MÜHENDİSLİĞİ SİSTEM PROGRAMLAMA 2025-2026 BAHAR DÖNEMİ PROJESİ
Teslim tarihi: 24 Mayıs 2026 23:59
Bu çalışmada, tar, rar veya zip gibi çalışan ancak sıkıştırma yapmayan "tarsau" adlı bir arşivleme programı oluşturmanız istenmektedir. Program, Linux (veya Unix) işletim sisteminde C dilinde yazılacak ve aşağıdaki şekilde gösterildiği gibi komut satırından çalıştırılacaktır.


-- tarsau –b :
•	Giriş dosyaları yalnızca metin dosyaları olabilir (ASCII, karakter başına 1 bayt). Yukarıdaki örnekte, t1, t2, t3, t4.txt ve t5.dat dosyalarının hepsi metin dosyalarıdır.
•	Program yalnızca metin dosyalarını birleştirmeli ve tek bir metin dosyasına dönüştürmelidir.
•	Çıktı dosyasının formatı aşağıda verilmiştir.
•	Arşiv dosyasının adı, –o parametresinden sonra belirtilmelidir.
•	Arşiv dosya adı belirtilmezse, varsayılan olarak a.sau kullanılır.
•	Giriş dosyalarının toplam boyutu 200 MB'ı geçemez.
•	Giriş dosyası sayısı en fazla 32 olabilir.
•	Hatalı formatta bir giriş dosyası verildiğinde, Örneğin t7 dosyası uyumsuz ise,  “t7 giriş dosyasının formatı uyumsuzdur!" mesajı yazılmalı ve sorunsuz bir şekilde programdan çıkılmalıdır.
-- tarsau –a :
•	-a parametresinden sonra en fazla 2 parametre almalıdır.
•	-a parametresinden sonraki ilk parametre, arşiv dosyasının adı olmalıdır (*.sau). Uygun olmayan bir dosya adı girildiğinde... 'Arşiv dosyası uygunsuz veya bozuk!' mesajı yazılmalıdır.
•	-a parametresinden sonraki ikinci parametre dizin adı parametresidir. Arşiv dosyasının geçerli dizinde açılması isteniyorsa, dışa aktarılmayabilir.
•	İndeks parametresi göreceli veya mutlak olabilir.
•	Girilen dizin adında boş yer yoksa, önce dizin oluşturulur ve ardından dosyalar bu dizine yerleştirilir.
•	Tüm çıkışlar sorunsuz olmalı, program aniden çökmemeli.
•	Açılan dosyaların, açıldıkları zamankiyle aynı izinlere (okuma, yazma, çalıştırma izinleri) sahip olması gerekir. Arşivleme.
 
-- sau arşiv dosyası formatı:
Arşiv dosyası iki bölümden oluşur: 1) Organizasyon (içerik) bilgileri, 2) Arşivlenmiş dosyalar.
1.	Organizasyon (içerik) bölümü: 
•	• İlk 10 bayt, ilk bölümün ASCII formatındaki sayısal boyutunu içerir.
•	Sonraki her kayıt '|' ile ayrılır.
•	Kayıttaki alanlar virgülle ayrılmıştır ve şunlardır: |Dosya adı, izinler, boyut|
•	Arşiv dosyaları, son kaydın bitmesinin hemen ardından oluşturulmaya başlanır.
2) Arşivlenmiş dosyalar: 
• Arşivlenmiş dosyalar, herhangi bir ayırıcı kullanılmadan ASCII formatında art arda yerleştirilir.
•	Son karakter dosya sonunu belirtir.

Proje ile ilgili notlar:
*	Proje, make dosyasıyla derlenebilmesi için sisteme yüklenmelidir.
*	Proje iki kişilik bir ekip olarak geliştirilebilir.
*	Projenin geliştirme sürecini açıklayan, uygulamanın kod parçacıklarını ve ekran çıktılarını da içeren bir rapor hazırlanmalıdır.
*	Projenin geliştirilmesi: Projenin geliştirme aşamasında, proje ekibinin bir üyesi https://github.com/ adresinde proje için bir kayıt oluşturacak ve geliştirme süreçleri bu adres üzerinden yürütülecektir. Bu adres nihai rapora eklenecektir.
*	Sisteme Yükleme: Ödev içeriğinde yer alan tüm belgeler (proje kodları, rapor dosyası), tek bir klasöre kopyalanıp sıkıştırıldıktan sonra sisteme tek parça halinde yüklenecektir (klasörün adı öğrenci numaralarınız olmalıdır). (b211210095_b211210024.rar/zip)
