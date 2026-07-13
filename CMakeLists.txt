cmake_minimum_required(VERSION 3.16)
project(PSForcerCekirdegi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(psforcer_cekirdegi
    src/JsonTurkce.cpp
    src/Catalog.cpp
    src/Sha256.cpp
    src/FileUtil.cpp
    src/HttpClientTurkce.cpp
    src/DownloadManager.cpp
    src/MediaCache.cpp
    src/Installer.cpp
)
target_include_directories(psforcer_cekirdegi PUBLIC src)

add_executable(katalog_cozumleme_sinavi tests/catalog_parser_test.cpp)
target_link_libraries(katalog_cozumleme_sinavi PRIVATE psforcer_cekirdegi)

enable_testing()
add_test(NAME katalog_cozumleme_sinavi
         COMMAND katalog_cozumleme_sinavi ${CMAKE_SOURCE_DIR}/assets/catalog.json)

add_executable(indirme_yoneticisi_sinavi tests/download_manager_test.cpp)
target_link_libraries(indirme_yoneticisi_sinavi PRIVATE psforcer_cekirdegi)
add_test(NAME indirme_yoneticisi_sinavi COMMAND indirme_yoneticisi_sinavi)
