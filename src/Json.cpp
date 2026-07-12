#include "Json.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace psforcer {

namespace {
const std::string kEmptyString;
const std::vector<JsonValue> kEmptyArray;
const std::map<std::string, JsonValue> kEmptyObject;

void appendUtf8(std::string& out, unsigned int codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}  // namespace

JsonValue::JsonValue() : type_(Type::Null), bool_(false), number_(0.0) {}

bool JsonValue::boolValue(bool fallback) const { return isBool() ? bool_ : fallback; }
double JsonValue::numberValue(double fallback) const { return isNumber() ? number_ : fallback; }
const std::string& JsonValue::stringValue() const { return isString() ? string_ : kEmptyString; }
const std::vector<JsonValue>& JsonValue::arrayValue() const { return isArray() ? array_ : kEmptyArray; }
const std::map<std::string, JsonValue>& JsonValue::objectValue() const { return isObject() ? object_ : kEmptyObject; }

const JsonValue* JsonValue::get(const std::string& key) const {
    if (!isObject()) return NULL;
    std::map<std::string, JsonValue>::const_iterator it = object_.find(key);
    return it == object_.end() ? NULL : &it->second;
}

bool JsonParser::parse(const std::string& text, JsonValue& output, std::string& error) {
    text_ = &text;
    position_ = 0;
    error_ = &error;
    error.clear();
    skipWhitespace();
    if (!parseValue(output)) return false;
    skipWhitespace();
    if (position_ != text_->size()) return fail("Unexpected trailing JSON data");
    return true;
}

bool JsonParser::parseValue(JsonValue& value) {
    skipWhitespace();
    const char c = peek();
    if (c == '{') return parseObject(value);
    if (c == '[') return parseArray(value);
    if (c == '"') {
        value = JsonValue();
        value.type_ = JsonValue::Type::String;
        return parseString(value.string_);
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(value);
    if (c == 't') {
        if (!matchLiteral("true")) return false;
        value = JsonValue();
        value.type_ = JsonValue::Type::Boolean;
        value.bool_ = true;
        return true;
    }
    if (c == 'f') {
        if (!matchLiteral("false")) return false;
        value = JsonValue();
        value.type_ = JsonValue::Type::Boolean;
        value.bool_ = false;
        return true;
    }
    if (c == 'n') {
        if (!matchLiteral("null")) return false;
        value = JsonValue();
        return true;
    }
    return fail("Expected JSON value");
}

bool JsonParser::parseObject(JsonValue& value) {
    if (get() != '{') return fail("Expected object");
    value = JsonValue();
    value.type_ = JsonValue::Type::Object;
    skipWhitespace();
    if (peek() == '}') {
        get();
        return true;
    }
    while (true) {
        skipWhitespace();
        std::string key;
        if (!parseString(key)) return false;
        skipWhitespace();
        if (get() != ':') return fail("Expected ':' after object key");
        JsonValue child;
        if (!parseValue(child)) return false;
        value.object_[key] = child;
        skipWhitespace();
        const char separator = get();
        if (separator == '}') return true;
        if (separator != ',') return fail("Expected ',' or '}' in object");
    }
}

bool JsonParser::parseArray(JsonValue& value) {
    if (get() != '[') return fail("Expected array");
    value = JsonValue();
    value.type_ = JsonValue::Type::Array;
    skipWhitespace();
    if (peek() == ']') {
        get();
        return true;
    }
    while (true) {
        JsonValue child;
        if (!parseValue(child)) return false;
        value.array_.push_back(child);
        skipWhitespace();
        const char separator = get();
        if (separator == ']') return true;
        if (separator != ',') return fail("Expected ',' or ']' in array");
    }
}

bool JsonParser::parseString(std::string& value) {
    if (get() != '"') return fail("Expected JSON string");
    value.clear();
    while (position_ < text_->size()) {
        char c = get();
        if (c == '"') return true;
        if (static_cast<unsigned char>(c) < 0x20) return fail("Control character in JSON string");
        if (c != '\\') {
            value.push_back(c);
            continue;
        }
        if (position_ >= text_->size()) return fail("Unterminated JSON escape");
        const char escaped = get();
        switch (escaped) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u': {
                if (position_ + 4 > text_->size()) return fail("Incomplete Unicode escape");
                unsigned int codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    const int digit = hexDigit(get());
                    if (digit < 0) return fail("Invalid Unicode escape");
                    codepoint = (codepoint << 4) | static_cast<unsigned int>(digit);
                }
                appendUtf8(value, codepoint);
                break;
            }
            default: return fail("Invalid JSON escape");
        }
    }
    return fail("Unterminated JSON string");
}

bool JsonParser::parseNumber(JsonValue& value) {
    const size_t start = position_;
    if (peek() == '-') get();
    if (peek() == '0') {
        get();
    } else {
        if (peek() < '1' || peek() > '9') return fail("Invalid JSON number");
        while (peek() >= '0' && peek() <= '9') get();
    }
    if (peek() == '.') {
        get();
        if (peek() < '0' || peek() > '9') return fail("Invalid fractional JSON number");
        while (peek() >= '0' && peek() <= '9') get();
    }
    if (peek() == 'e' || peek() == 'E') {
        get();
        if (peek() == '+' || peek() == '-') get();
        if (peek() < '0' || peek() > '9') return fail("Invalid exponent");
        while (peek() >= '0' && peek() <= '9') get();
    }
    const std::string token = text_->substr(start, position_ - start);
    errno = 0;
    char* end = NULL;
    const double parsed = std::strtod(token.c_str(), &end);
    if (errno != 0 || end == token.c_str() || *end != '\0' || !std::isfinite(parsed)) return fail("Invalid JSON number");
    value = JsonValue();
    value.type_ = JsonValue::Type::Number;
    value.number_ = parsed;
    return true;
}

bool JsonParser::matchLiteral(const char* literal) {
    const size_t start = position_;
    while (*literal) {
        if (get() != *literal++) {
            position_ = start;
            return fail("Invalid JSON literal");
        }
    }
    return true;
}

bool JsonParser::fail(const std::string& message) {
    std::ostringstream stream;
    stream << message << " at byte " << position_;
    *error_ = stream.str();
    return false;
}

void JsonParser::skipWhitespace() {
    while (position_ < text_->size()) {
        const char c = (*text_)[position_];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') ++position_;
        else break;
    }
}

char JsonParser::peek() const { return position_ < text_->size() ? (*text_)[position_] : '\0'; }
char JsonParser::get() { return position_ < text_->size() ? (*text_)[position_++] : '\0'; }

}  // namespace psforcer
