# PSForcer

PSForcer, OpenOrbis tabanlı bir PlayStation 4 katalog ve indirme istemcisidir. Oyunları konsola uygun bir mağaza görünümünde sunar; ana oyun, güncelleme, ek paket ve ekstra dosyalarını ayırır; katalogda belirtilen bağlantılardan içerik indirir; SHA-256 bütünlüğünü denetler ve tamamlanan dosyayı kurucu uyarlayıcısına teslim eder.

> Bu depo; açık, güvenlik açığı, jailbreak aracı, Sony SDK içeriği, kapalı kaynak çalışma modülü veya platform güvenliğini aşan kod içermez. Yerleşik kurucu uyarlayıcısı, doğrulanmış paketi elle ya da yetkili yöntemle kurulmaya hazır olarak işaretler. Otomatik kurulum yalnızca hedef ortamda meşru biçimde kullanılabilen bir kurucu arayüzüne bağlanabilir.

## Güncel aşama

- 1920×1080 çözünürlüklü SDL2 mağaza arayüzü
- Liste ve Steam benzeri geniş başlıklı ayrıntı ekranları
- Ana oyun, güncelleme, ek paket ve ekstra gruplaması
- Yerel ve uzak kapak, geniş görsel ve ekran görüntüsü alanları
- Yalnızca ekranda ihtiyaç duyulan uzak görseller için geçici medya önbelleği
- Paket içinden yüklenen ve GitHub üzerinden açılışta otomatik yenilenen JSON kataloğu
- Sürdürülebilir arka plan HTTP(S) indirmeleri
- Kurucuya teslimden önce SHA-256 bütünlük denetimi
- Yalnızca kurucu başarı bildirdikten sonra çalışan kurulum sonrası silme seçeneği
- Hugging Face herkese açık ve yetkili salt-okunur erişim desteği

## Denetimler

| Düğme | İşlem |
|---|---|
| Yön düğmeleri | Gezinme |
| L1 / R1 | Katalog filtresini değiştirme |
| Çarpı | Oyun ayrıntılarını açma |
| Daire | Geri dönme |
| Kare | Seçili paketi indirme |
| Üçgen | Uzak kataloğu yenileme |
| Seçenekler | Çıkış |

## OpenOrbis ile derleme

1. OpenOrbis PS4 Araç Zinciri'ni kurun ve `OO_PS4_TOOLCHAIN` değişkenini ayarlayın.
2. Bir kez `make bootstrap` çalıştırın. Bu işlem, paket çalışma modüllerini ve `right.sprx` dosyasını yerel OpenOrbis SDL2 örneğinden kopyalar. Bu ikili dosyalar bilerek depoya eklenmez.
3. `make` çalıştırın. Paket simgesi kaynak koddan otomatik oluşturulur.
4. Üretilen PKG dosyasını sınama ortamınızda bulunan yetkili yöntemle kurun.

```sh
export OO_PS4_TOOLCHAIN=/OpenOrbis-PS4-Toolchain/yolu
make bootstrap
make
```

Üretilen paket adı:

```text
IV0000-PSFC00001_00-PSFORCERCLIENT00.pkg
```

## Uzak katalog ayarı

Konsolda şu metin dosyasını oluşturun:

```text
/data/psforcer/katalog_adresi.txt
```

Dosyaya tek bir HTTPS bağlantısı yazın. Paket içinde boş bir örnek dosya da bulunur. PSForcer içinde Üçgen düğmesine basıldığında katalog `/data/psforcer/katalog.json` konumuna indirilir ve yeniden yüklenir.

PKG dosyaları Hugging Face üzerinde tutulur. Katalog, açıklamalar ve medya bağlantıları GitHub’daki `catalog/katalog.json` dosyasından alınır. Her paket kaydı farklı bir Hugging Face deposundaki doğrudan `resolve` bağlantısına bağlanabilir.

Katalog yapısı için [docs/MANIFEST.md](docs/MANIFEST.md) belgesine bakın.

## Hugging Face erişimi

Herkese açık Hugging Face dosyaları ek ayar gerektirmez. Bir paket bağlantısı HTTP `401` döndürürse depo özel, erişim denetimli veya kullanılan hesabın yetkisi dışında olabilir. Yetkili kullanıcı, yalnızca okuma izni olan Hugging Face belirtecini PS4 üzerinde aşağıdaki dosyaya tek satır olarak yazabilir:

```text
/data/psforcer/hf_token.txt
```

Belirteç GitHub kataloğuna, uygulama paketine, ekran görüntülerine veya hata kayıtlarına eklenmemelidir. PSForcer bu değeri yalnızca `https://huggingface.co/` isteklerinde `Authorization: Bearer` başlığı olarak kullanır. Erişim izni olmayan bir depo bu yöntemle aşılamaz; depo sahibi dosyayı herkese açık yapmalı veya kullanıcıya meşru okuma izni vermelidir.

## Bilgisayarda doğrulama

Katalog çözümleyicisi ve SHA-256 uygulaması platformdan bağımsızdır:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tools/validate_catalog.py assets/catalog.json
```

## Proje durumu

Bu sürümde P.T. kapak, header ve ekran görüntüleri paket içinde bulunduğu için dış görsel sunucularına bağlı değildir. Diğer katalog kayıtları yerel veya uzak medya bağlantıları kullanabilir. Video alanı veri modelinde bulunur ancak oynatıcı henüz bağlı değildir. Paket indirme, devam ettirme, beklenen boyut denetimi, yetkili Hugging Face erişimi ve isteğe bağlı SHA-256 bütünlük denetimi çalışır; kurulum işlemi açıkça ayrılmış kurucu arayüzünün arkasındadır. Yeni oyunlar `catalog/katalog.json` güncellenerek eklenir ve uygulama kodu değişmediği sürece yeni PSForcer PKG sürümü gerektirmez.
