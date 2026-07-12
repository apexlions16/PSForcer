from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"{label} bulunamadı")
    return text.replace(old, new, 1)


path = Path("src/HttpClientTurkce.cpp")
text = path.read_text()

marker = "Keep-alive yanıtları PS4'te burada süresiz bekletebiliyor"
if marker not in text:
    anchor = "        bool readFailed = false;\n        while (true) {\n"
    insertion = (
        "            // Bilinen dosya boyutuna veya bu HTTP yanıtının bildirilen\n"
        "            // uzunluğuna ulaşıldığında sunucunun bağlantıyı kapatmasını\n"
        "            // bekleme. Keep-alive yanıtları PS4'te burada süresiz bekletebiliyor.\n"
        "            if (expectedSize > 0 && downloaded == expectedSize) {\n"
        "                success = true;\n"
        "                break;\n"
        "            }\n"
        "            if (expectedResponseBytes > 0 &&\n"
        "                responseBytes == expectedResponseBytes) {\n"
        "                break;\n"
        "            }\n\n"
    )
    text = replace_once(text, anchor, anchor + insertion, "PS4 okuma döngüsü")

# sceHttpReadData bazı PS4 ortamlarında istenen tamponun tamamı dolana veya
# bağlantı kapanana kadar bekleyebiliyor. Son parça 512 KiB'den küçük olduğunda
# dosya ağdan tamamen gelmiş görünse bile keep-alive bağlantısı indirme işçisini
# içeride tutuyordu. Her çağrıyı dosyada ve HTTP yanıtında kalan kesin baytla
# sınırlandır; böylece son çağrı tam kalan uzunluğu ister ve EOF beklemez.
if "uint64_t readCapacity = buffer.size();" not in text:
    text = replace_once(
        text,
        "            const int read = sceHttpReadData(requestId, &buffer[0],\n"
        "                                             static_cast<uint32_t>(buffer.size()));",
        "            uint64_t readCapacity = buffer.size();\n"
        "            if (expectedSize > 0) {\n"
        "                readCapacity = std::min<uint64_t>(\n"
        "                    readCapacity, expectedSize - downloaded);\n"
        "            }\n"
        "            if (expectedResponseBytes > 0) {\n"
        "                readCapacity = std::min<uint64_t>(\n"
        "                    readCapacity, expectedResponseBytes - responseBytes);\n"
        "            }\n"
        "            if (readCapacity == 0) break;\n\n"
        "            const int read = sceHttpReadData(\n"
        "                requestId, &buffer[0], static_cast<uint32_t>(readCapacity));",
        "PS4 sınırlı okuma çağrısı",
    )

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

path.write_text(text)
print("HTTP kesin tamamlama yaması uygulandı")
