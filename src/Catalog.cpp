#include "Catalog.h"
#include "Json.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace psforcer {

namespace {
std::string readTextFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) return std::string();
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}
std::string getString(const JsonValue& object, const char* key, const std::string& fallback = std::string()) {
    const JsonValue* value = object.get(key);
    return value && value->isString() ? value->stringValue() : fallback;
}
int getInt(const JsonValue& object, const char* key, int fallback = 0) {
    const JsonValue* value = object.get(key);
    return value && value->isNumber() ? static_cast<int>(value->numberValue()) : fallback;
}
uint64_t getUint64(const JsonValue& object, const char* key, uint64_t fallback = 0) {
    const JsonValue* value = object.get(key);
    if (!value || !value->isNumber() || value->numberValue() < 0.0) return fallback;
    return static_cast<uint64_t>(value->numberValue());
}
bool getBool(const JsonValue& object, const char* key, bool fallback = false) {
    const JsonValue* value = object.get(key);
    return value && value->isBool() ? value->boolValue() : fallback;
}
int hexPair(const std::string& text, size_t offset) {
    if (offset + 2 > text.size()) return -1;
    int value = 0;
    for (size_t i = offset; i < offset + 2; ++i) {
        const char c = text[i];
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        if (digit < 0) return -1;
        value = value * 16 + digit;
    }
    return value;
}
Color parseColor(std::string value) {
    if (!value.empty() && value[0] == '#') value.erase(0, 1);
    if (value.size() != 6) return Color(108, 99, 255, 255);
    const int r = hexPair(value, 0);
    const int g = hexPair(value, 2);
    const int b = hexPair(value, 4);
    if (r < 0 || g < 0 || b < 0) return Color(108, 99, 255, 255);
    return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255);
}
bool validateRequiredString(const JsonValue& object, const char* key, const std::string& context, std::string& error) {
    const JsonValue* value = object.get(key);
    if (!value || !value->isString() || value->stringValue().empty()) {
        error = context + " requires non-empty string '" + key + "'";
        return false;
    }
    return true;
}
}  // namespace

bool CatalogLoader::loadFile(const std::string& path, CatalogData& catalog, std::string& error) {
    const std::string json = readTextFile(path);
    if (json.empty()) {
        error = "Catalog file is missing or empty: " + path;
        return false;
    }
    return parse(json, catalog, error);
}

bool CatalogLoader::parse(const std::string& json, CatalogData& catalog, std::string& error) {
    JsonValue root;
    JsonParser parser;
    if (!parser.parse(json, root, error)) return false;
    if (!root.isObject()) {
        error = "Catalog root must be an object";
        return false;
    }
    CatalogData parsed;
    parsed.schemaVersion = getInt(root, "schemaVersion", 0);
    parsed.catalogTitle = getString(root, "catalogTitle", "PSForcer");
    parsed.updatedAt = getString(root, "updatedAt");
    if (parsed.schemaVersion != 1) {
        error = "Unsupported schemaVersion; expected 1";
        return false;
    }
    const JsonValue* items = root.get("items");
    if (!items || !items->isArray()) {
        error = "Catalog requires an items array";
        return false;
    }
    const std::vector<JsonValue>& itemValues = items->arrayValue();
    for (size_t itemIndex = 0; itemIndex < itemValues.size(); ++itemIndex) {
        const JsonValue& source = itemValues[itemIndex];
        std::ostringstream context;
        context << "items[" << itemIndex << "]";
        if (!source.isObject()) {
            error = context.str() + " must be an object";
            return false;
        }
        if (!validateRequiredString(source, "id", context.str(), error) ||
            !validateRequiredString(source, "title", context.str(), error)) return false;
        CatalogItem item;
        item.id = getString(source, "id");
        item.title = getString(source, "title");
        item.developer = getString(source, "developer", "Unknown studio");
        item.genre = getString(source, "genre", "Other");
        item.description = getString(source, "description");
        item.releaseYear = getInt(source, "releaseYear", 0);
        item.accent = parseColor(getString(source, "accent", "6C63FF"));
        const JsonValue* media = source.get("media");
        if (media && media->isObject()) {
            item.media.cover = getString(*media, "cover");
            item.media.hero = getString(*media, "hero");
            item.media.trailer = getString(*media, "trailer");
            const JsonValue* screenshots = media->get("screenshots");
            if (screenshots && screenshots->isArray()) {
                const std::vector<JsonValue>& screenshotValues = screenshots->arrayValue();
                for (size_t i = 0; i < screenshotValues.size(); ++i) {
                    if (screenshotValues[i].isString()) item.media.screenshots.push_back(screenshotValues[i].stringValue());
                }
            }
        }
        const JsonValue* packages = source.get("packages");
        if (!packages || !packages->isArray()) {
            error = context.str() + " requires a packages array";
            return false;
        }
        const std::vector<JsonValue>& packageValues = packages->arrayValue();
        for (size_t packageIndex = 0; packageIndex < packageValues.size(); ++packageIndex) {
            const JsonValue& packageSource = packageValues[packageIndex];
            std::ostringstream packageContext;
            packageContext << context.str() << ".packages[" << packageIndex << "]";
            if (!packageSource.isObject()) {
                error = packageContext.str() + " must be an object";
                return false;
            }
            if (!validateRequiredString(packageSource, "id", packageContext.str(), error) ||
                !validateRequiredString(packageSource, "kind", packageContext.str(), error) ||
                !validateRequiredString(packageSource, "label", packageContext.str(), error)) return false;
            PackageInfo package;
            package.id = getString(packageSource, "id");
            package.kind = parseKind(getString(packageSource, "kind"));
            if (package.kind == PackageKind::Unknown) {
                error = packageContext.str() + " has invalid package kind";
                return false;
            }
            package.label = getString(packageSource, "label");
            package.version = getString(packageSource, "version", "1.00");
            package.sizeBytes = getUint64(packageSource, "size", 0);
            package.url = getString(packageSource, "url");
            package.sha256 = getString(packageSource, "sha256");
            package.minFirmware = getString(packageSource, "minFirmware");
            package.deleteAfterInstall = getBool(packageSource, "deleteAfterInstall", false);
            item.packages.push_back(package);
        }
        parsed.items.push_back(item);
    }
    catalog = parsed;
    return true;
}

bool CatalogLoader::itemContainsKind(const CatalogItem& item, PackageKind kind) {
    for (size_t i = 0; i < item.packages.size(); ++i) {
        if (item.packages[i].kind == kind) return true;
    }
    return false;
}

PackageKind CatalogLoader::parseKind(const std::string& value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), static_cast<int(*)(int)>(std::tolower));
    if (normalized == "game" || normalized == "base") return PackageKind::Game;
    if (normalized == "update" || normalized == "patch") return PackageKind::Update;
    if (normalized == "dlc") return PackageKind::Dlc;
    if (normalized == "extra" || normalized == "bonus") return PackageKind::Extra;
    return PackageKind::Unknown;
}

std::string CatalogLoader::formatBytes(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << ' ' << units[unit];
    return stream.str();
}

}  // namespace psforcer
