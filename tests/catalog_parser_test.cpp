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
    if (catalog.schemaVersion != 1 || catalog.items.size() != 3) {
        std::cerr << "Katalog yapısı beklenenden farklı\n";
        return 1;
    }
    if (!psforcer::CatalogLoader::itemContainsKind(catalog.items[0], psforcer::PackageKind::Update)) {
        std::cerr << "Güncelleme paketi çözümlenemedi\n";
        return 1;
    }
    psforcer::Sha256 sha;
    sha.update(std::string("abc"));
    if (sha.finalHex() != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
        std::cerr << "SHA-256 öz sınaması başarısız\n";
        return 1;
    }
    std::cout << "Katalog çözümleme ve SHA-256 sınamaları geçti\n";
    return 0;
}
