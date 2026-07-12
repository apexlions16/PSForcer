#pragma once

#include <stdint.h>
#include <string>

namespace psforcer {

bool fileExists(const std::string& path);
uint64_t fileSize(const std::string& path);
bool ensureDirectory(const std::string& path);
std::string readFirstLine(const std::string& path);
std::string sanitizeFileName(const std::string& value);
std::string trim(const std::string& value);

}  // namespace psforcer
