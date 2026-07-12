#include "FileUtil.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sys/stat.h>

namespace psforcer {

bool fileExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

uint64_t fileSize(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) return 0;
    return static_cast<uint64_t>(info.st_size);
}

bool ensureDirectory(const std::string& path) {
    if (path.empty()) return false;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        current.push_back(c);
        if (c != '/' || current.size() == 1) continue;
        if (mkdir(current.c_str(), 0777) != 0 && !fileExists(current)) return false;
    }
    if (mkdir(current.c_str(), 0777) != 0 && !fileExists(current)) return false;
    return true;
}

std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string readFirstLine(const std::string& path) {
    std::ifstream input(path.c_str());
    if (!input) return std::string();
    std::string line;
    std::getline(input, line);
    return trim(line);
}

std::string sanitizeFileName(const std::string& value) {
    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.') result.push_back(static_cast<char>(c));
        else result.push_back('_');
    }
    return result.empty() ? "download" : result;
}

}  // namespace psforcer
