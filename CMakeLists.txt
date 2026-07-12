cmake_minimum_required(VERSION 3.16)
project(PSForcerCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(psforcer_core
    src/Json.cpp
    src/Catalog.cpp
    src/Sha256.cpp
    src/FileUtil.cpp
    src/HttpClient.cpp
    src/DownloadManager.cpp
    src/Installer.cpp
)
target_include_directories(psforcer_core PUBLIC src)

add_executable(catalog_parser_test tests/catalog_parser_test.cpp)
target_link_libraries(catalog_parser_test PRIVATE psforcer_core)

enable_testing()
add_test(NAME catalog_parser_test
         COMMAND catalog_parser_test ${CMAKE_SOURCE_DIR}/assets/catalog.json)
