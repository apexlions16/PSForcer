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

# Uzun paket aktarımında bağlantının açık kalmasına güvenme. Her HTTP yanıtı
# tamamlandığında soket kapansın; yeniden bağlantı gerekiyorsa istemci kesin
# dosya ofsetinden yeni bir aralık isteği açar.
text = text.replace(
    '            sceHttpAddRequestHeader(requestId, "Connection", "keep-alive", 0);',
    '            sceHttpAddRequestHeader(requestId, "Connection", "close", 0);',
    1,
)

# Kaldığı yerden devam isteğini dosyanın katalogdaki son baytıyla sınırla.
# Sunucu daha büyük bir nesne barındırsa bile istek hedef PKG dışına taşamaz.
if 'range << (expectedSize - 1);' not in text:
    text = replace_once(
        text,
        '                range << "bytes=" << requestStart << \'-\';',
        '                range << "bytes=" << requestStart << \'-\';\n'
        '                if (expectedSize > 0) range << (expectedSize - 1);',
        "sınırlı Range başlığı",
    )

# Hugging Face yetkilendirme hatasını doğrudan token dosyasına bağla.
if "/data/psforcer/hf_token.txt" not in text[text.find("if (statusCode != 200"):]:
    text = replace_once(
        text,
        '        if (statusCode != 200 && statusCode != 206) {\n'
        '            std::ostringstream message;\n'
        '            message << "İndirme isteği başarısız oldu; durum kodu: " << statusCode;\n'
        '            error = message.str();\n'
        '            closeRequestObjects(requestId, connectionId);\n'
        '            sceHttpDeleteTemplate(templateId);\n'
        '            break;\n'
        '        }',
        '        if (statusCode != 200 && statusCode != 206) {\n'
        '            std::ostringstream message;\n'
        '            if (statusCode == 401 && isHuggingFaceUrl(currentUrl)) {\n'
        '                message << "Hugging Face erişimi reddedildi. Tokenı "\n'
        '                        << "/data/psforcer/hf_token.txt dosyasının ilk satırına yazın";\n'
        '            } else {\n'
        '                message << "İndirme isteği başarısız oldu; durum kodu: " << statusCode;\n'
        '            }\n'
        '            error = message.str();\n'
        '            closeRequestObjects(requestId, connectionId);\n'
        '            sceHttpDeleteTemplate(templateId);\n'
        '            break;\n'
        '        }',
        "Hugging Face 401 açıklaması",
    )

# Sunucu başlıkları katalogdaki kesin boyutla uyuşmuyorsa veri yazmaya başlamadan
# dur. Böylece yanlış bağlantı, HTML hata gövdesi veya farklı bir PKG diski
# dolduramaz.
size_guard_marker = "Sunucu dosya boyutu katalogla uyuşmuyor"
if size_guard_marker not in text:
    anchor = "        if (expectedSize > 0) total = expectedSize;\n\n"
    insertion = (
        "        if (expectedSize > 0) {\n"
        "            bool sizeMismatch = false;\n"
        "            uint64_t reportedSize = 0;\n"
        "            if (contentRange.valid && contentRange.total > 0) {\n"
        "                reportedSize = contentRange.total;\n"
        "                sizeMismatch = reportedSize != expectedSize;\n"
        "            } else if (!rangeRequested && expectedResponseBytes > 0) {\n"
        "                reportedSize = expectedResponseBytes;\n"
        "                sizeMismatch = reportedSize != expectedSize;\n"
        "            }\n"
        "            if (sizeMismatch) {\n"
        "                std::ostringstream message;\n"
        "                message << \"Sunucu dosya boyutu katalogla uyuşmuyor. Beklenen \"\n"
        "                        << expectedSize << \" bayt, bildirilen \" << reportedSize;\n"
        "                error = message.str();\n"
        "                closeRequestObjects(requestId, connectionId);\n"
        "                sceHttpDeleteTemplate(templateId);\n"
        "                break;\n"
        "            }\n"
        "        }\n\n"
    )
    text = replace_once(text, anchor, anchor + insertion, "sunucu boyut doğrulaması")

# sceHttpReadData bazı PS4 ortamlarında istenen tamponun tamamı dolana veya
# bağlantı kapanana kadar bekleyebiliyor. Son parça 512 KiB'den küçük olduğunda
# dosya ağdan tamamen gelmiş görünse bile bağlantı indirme işçisini içeride
# tutuyordu. Her çağrıyı dosyada ve HTTP yanıtında kalan kesin baytla sınırla.
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
print("HTTP kesin boyut, temiz bağlantı ve tamamlama yaması uygulandı")
