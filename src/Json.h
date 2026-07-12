#pragma once

#include <map>
#include <string>
#include <vector>

namespace psforcer {

class JsonValue {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };
    JsonValue();
    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Boolean; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    bool boolValue(bool fallback = false) const;
    double numberValue(double fallback = 0.0) const;
    const std::string& stringValue() const;
    const std::vector<JsonValue>& arrayValue() const;
    const std::map<std::string, JsonValue>& objectValue() const;
    const JsonValue* get(const std::string& key) const;
private:
    friend class JsonParser;
    Type type_;
    bool bool_;
    double number_;
    std::string string_;
    std::vector<JsonValue> array_;
    std::map<std::string, JsonValue> object_;
};

class JsonParser {
public:
    bool parse(const std::string& text, JsonValue& output, std::string& error);
private:
    bool parseValue(JsonValue& value);
    bool parseObject(JsonValue& value);
    bool parseArray(JsonValue& value);
    bool parseString(std::string& value);
    bool parseNumber(JsonValue& value);
    bool matchLiteral(const char* literal);
    bool fail(const std::string& message);
    void skipWhitespace();
    char peek() const;
    char get();
    const std::string* text_;
    size_t position_;
    std::string* error_;
};

}  // namespace psforcer
