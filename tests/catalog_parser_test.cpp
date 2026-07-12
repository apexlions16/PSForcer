#include "Catalog.h"
#include "Sha256.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Katalog yolu gerekli\n";
        return 2;
    }

    psforcer::CatalogData catalog;
    std::string error;
    if (!psforcer::CatalogLoader::loadFile(argv[1], catalog, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    if (catalog.schemaVersion != 1 || catalog.items.size() != 1) {
        std::cerr << "Katalog yapısı beklenenden farklı\n";
        return 1;
    }

    const psforcer::CatalogItem& oyun = catalog.items[0];
    if (oyun.id != "pt-cusa01127-usa" || oyun.title != "P.T.") {
        std::cerr << "P.T. katalog kaydı çözümlenemedi\n";
        return 1;
    }
    if (oyun.packages.size() != 1 || oyun.packages[0].kind != psforcer::PackageKind::Game) {
        std::cerr << "P.T. ana oyun paketi çözümlenemedi\n";
        return 1;
    }
    if (oyun.packages[0].sizeBytes != 1478164480ULL) {
        std::cerr << "P.T. paket boyutu beklenenden farklı\n";
        return 1;
    }
    if (oyun.packages[0].url != "https://huggingface.co/datasets/glm-labs/entroylab/resolve/main/PPCH4/CUSA01127-USA.pkg") {
        std::cerr << "P.T. indirme bağlantısı beklenenden farklı\n";
        return 1;
    }

    psforcer::Sha256 sha;
    sha.update(std::string("abc"));
    if (sha.finalHex() != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
        std::cerr << "SHA-256 öz sınaması başarısız\n";
        return 1;
    }

    std::cout << "P.T. katalog ve SHA-256 sınamaları geçti\n";
    return 0;
}
