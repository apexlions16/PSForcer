#include "Catalog.h"
#include "Sha256.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "catalog path required\n";
        return 2;
    }
    psforcer::CatalogData catalog;
    std::string error;
    if (!psforcer::CatalogLoader::loadFile(argv[1], catalog, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (catalog.schemaVersion != 1 || catalog.items.size() != 3) {
        std::cerr << "unexpected catalog shape\n";
        return 1;
    }
    if (!psforcer::CatalogLoader::itemContainsKind(catalog.items[0], psforcer::PackageKind::Update)) {
        std::cerr << "update package was not parsed\n";
        return 1;
    }
    psforcer::Sha256 sha;
    sha.update(std::string("abc"));
    if (sha.finalHex() != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
        std::cerr << "SHA-256 self-test failed\n";
        return 1;
    }
    std::cout << "catalog parser and SHA-256 tests passed\n";
    return 0;
}
