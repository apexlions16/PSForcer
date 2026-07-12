from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"{label} bulunamadı")
    return text.replace(old, new, 1)


http_path = Path("src/HttpClientTurkce.cpp")
text = http_path.read_text()
lines = text.splitlines(True)

if "Keep-alive yanıtları PS4'te burada süresiz bekletebiliyor" not in text:
    target = "        bool readFailed = false;\n"
    try:
        index = lines.index(target)
    except ValueError as exc:
        raise RuntimeError("PS4 okuma döngüsü başlangıcı bulunamadı") from exc
    if index + 1 >= len(lines) or lines[index + 1] != "        while (true) {\n":
        raise RuntimeError("PS4 okuma döngüsü biçimi beklenenden farklı")

    insertion = [
        "            // Bilinen dosya boyutuna veya bu HTTP yanıtının bildirilen\n",
        "            // uzunluğuna ulaşıldığında sunucunun bağlantıyı kapatmasını\n",
        "            // bekleme. Keep-alive yanıtları PS4'te burada süresiz bekletebiliyor.\n",
        "            if (expectedSize > 0 && downloaded == expectedSize) {\n",
        "                success = true;\n",
        "                break;\n",
        "            }\n",
        "            if (expectedResponseBytes > 0 &&\n",
        "                responseBytes == expectedResponseBytes) {\n",
        "                break;\n",
        "            }\n",
        "\n",
    ]
    lines[index + 2:index + 2] = insertion
    text = "".join(lines)

text = text.replace(
    '#else\n    (void)expectedSize;\n    if (url.compare(0, 7, "file://") != 0) {',
    '#else\n    if (url.compare(0, 7, "file://") != 0) {',
    1,
)

if "const uint64_t sourceSize = fileSize(sourcePath);" not in text:
    text = replace_once(
        text,
        "    uint64_t existing = resume ? fileSize(destination) : 0;\n"
        "    const uint64_t total = fileSize(sourcePath);\n"
        "    if (existing > total) existing = 0;",
        "    uint64_t existing = resume ? fileSize(destination) : 0;\n"
        "    const uint64_t sourceSize = fileSize(sourcePath);\n"
        "    const uint64_t total = expectedSize > 0 ? expectedSize : sourceSize;\n"
        "    if (existing > total) existing = 0;",
        "Masaüstü toplam boyut bölümü",
    )

if "size_t wanted = buffer.size();" not in text:
    text = replace_once(
        text,
        "        const size_t read = std::fread(&buffer[0], 1, buffer.size(), input);",
        "        if (expectedSize > 0 && copied >= expectedSize) break;\n\n"
        "        size_t wanted = buffer.size();\n"
        "        if (expectedSize > 0) {\n"
        "            wanted = static_cast<size_t>(std::min<uint64_t>(\n"
        "                wanted, expectedSize - copied));\n"
        "        }\n"
        "        const size_t read = std::fread(&buffer[0], 1, wanted, input);",
        "Masaüstü okuma satırı",
    )

if "if (read < wanted) break;" not in text:
    text = replace_once(
        text,
        "        copied += read;\n"
        "        if (progress) progress(HttpProgress{copied, total});\n"
        "        if (read < buffer.size()) break;",
        "        copied += read;\n"
        "        if (progress) progress(HttpProgress{copied, total});\n"
        "        if (expectedSize > 0 && copied == expectedSize) break;\n"
        "        if (read < wanted) break;",
        "Masaüstü bitiş satırları",
    )

http_path.write_text(text)


test_path = Path("tests/download_manager_test.cpp")
test = test_path.read_text()
if "request.jobId = 3;" not in test:
    marker = "\n    manager.stopAndWait();"
    position = test.rfind(marker)
    if position < 0:
        raise RuntimeError("İndirme sınaması ekleme noktası bulunamadı")
    block = """
    manager.reset();

    // Kaynak beklenen boyuttan daha büyük olsa bile istemci tam hedef
    // baytına ulaştığında EOF beklemeden tamamlanmalıdır.
    std::remove(destination.c_str());
    request.jobId = 3;
    request.expectedSize = payload.size() - 12345;
    request.resume = false;
    if (!manager.start(request, error)) {
        std::cerr << error << '\\n';
        return 1;
    }
    snapshot = waitFor(manager);
    if (snapshot.state != psforcer::DownloadState::Completed ||
        psforcer::fileSize(destination) != request.expectedSize) {
        std::cerr << "Tam hedef boyutunda EOF beklemeden tamamlanamadı: "
                  << snapshot.error << '\\n';
        return 1;
    }
"""
    test = test[:position] + "\n" + block + test[position:]
    test_path.write_text(test)


make_path = Path("Makefile")
make = make_path.read_text()
make = make.replace("paket bilgileri - v0.22", "paket bilgileri - v0.23", 1)
make = make.replace("VERSION     := 0.22", "VERSION     := 0.23", 1)
make_path.write_text(make)


release_path = Path(".github/workflows/build-pkg-release.yml")
release = release_path.read_text()
release = release.replace("YAYIN_ETIKETI: v0.2.2", "YAYIN_ETIKETI: v0.2.3", 1)
release = release.replace("ESKI_YAYIN_ETIKETI: v0.2.1", "ESKI_YAYIN_ETIKETI: v0.2.2", 1)
release = release.replace(
    "PSForcer v0.2.2 Tek Oturumlu İndirme",
    "PSForcer v0.2.3 Kesin Tamamlama",
    1,
)
old_note = (
    "PS4 üzerinde güvenilir olmadığı görülen sceKernelPwrite yolu kaldırıldı. "
    "İndirme boyunca tek bir standart libc FILE akışı açık tutulur, tampon kapatılır "
    "ve bağlantı kesilse bile aynı kesin bellek konumundan Range isteğiyle devam edilir. "
    "stat yalnızca dosya kapatıldıktan sonra doğrulama amacıyla kullanılır; böylece "
    "bağlantı yenilemelerinde sayaç ve dosya konumu geriye dönmez."
)
new_note = (
    "İndirici, katalogdaki kesin dosya boyutuna veya HTTP Content-Length/Content-Range "
    "uzunluğuna ulaştığı anda sunucunun keep-alive bağlantısını kapatmasını beklemeden "
    "yanıtı tamamlar. Böylece ilerleme çubuğunun 1.4 GB / 1.4 GB noktasında "
    "İNDİRİLİYOR durumunda kalması giderildi. Tam boyutta EOF beklememe davranışı "
    "otomatik sınamayla doğrulanır."
)
release = release.replace(old_note, new_note, 1)
release_path.write_text(release)

print("v0.2.3 tamamlama düzeltmesi uygulandı")
