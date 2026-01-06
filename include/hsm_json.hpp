/**
 * @file hsm_json.hpp
 * @brief Lightweight JSON serialization utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - JSON value representation
 * - JSON parsing
 * - JSON serialization
 * - Type-safe accessors
 */

#ifndef HSM_JSON_HPP
#define HSM_JSON_HPP

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cmath>

namespace dfsmshsm {
namespace json {

// Forward declaration
class JsonValue;

// Type aliases
using JsonNull = std::monostate;
using JsonBool = bool;
using JsonNumber = double;
using JsonString = std::string;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

/**
 * @brief JSON value types
 */
enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

/**
 * @brief JSON value class
 */
class JsonValue {
public:
    using ValueType = std::variant<JsonNull, JsonBool, JsonNumber, JsonString, 
                                   JsonArray, JsonObject>;
    
    // Constructors
    JsonValue() : value_(JsonNull{}) {}
    JsonValue(std::nullptr_t) : value_(JsonNull{}) {}
    JsonValue(bool b) : value_(b) {}
    JsonValue(int i) : value_(static_cast<double>(i)) {}
    JsonValue(long l) : value_(static_cast<double>(l)) {}
    JsonValue(long long ll) : value_(static_cast<double>(ll)) {}
    JsonValue(double d) : value_(d) {}
    JsonValue(const char* s) : value_(JsonString(s)) {}
    JsonValue(std::string s) : value_(std::move(s)) {}
    JsonValue(std::string_view sv) : value_(JsonString(sv)) {}
    JsonValue(JsonArray arr) : value_(std::move(arr)) {}
    JsonValue(JsonObject obj) : value_(std::move(obj)) {}
    JsonValue(std::initializer_list<JsonValue> init);
    
    // Type checking
    [[nodiscard]] JsonType type() const noexcept;
    [[nodiscard]] bool isNull() const noexcept { return std::holds_alternative<JsonNull>(value_); }
    [[nodiscard]] bool isBool() const noexcept { return std::holds_alternative<JsonBool>(value_); }
    [[nodiscard]] bool isNumber() const noexcept { return std::holds_alternative<JsonNumber>(value_); }
    [[nodiscard]] bool isString() const noexcept { return std::holds_alternative<JsonString>(value_); }
    [[nodiscard]] bool isArray() const noexcept { return std::holds_alternative<JsonArray>(value_); }
    [[nodiscard]] bool isObject() const noexcept { return std::holds_alternative<JsonObject>(value_); }
    
    // Value access (throws on type mismatch)
    [[nodiscard]] bool asBool() const;
    [[nodiscard]] double asNumber() const;
    [[nodiscard]] int asInt() const;
    [[nodiscard]] int64_t asInt64() const;
    [[nodiscard]] const JsonString& asString() const;
    [[nodiscard]] const JsonArray& asArray() const;
    [[nodiscard]] const JsonObject& asObject() const;
    [[nodiscard]] JsonArray& asArray();
    [[nodiscard]] JsonObject& asObject();
    
    // Optional access (returns nullopt on type mismatch)
    [[nodiscard]] std::optional<bool> getBool() const noexcept;
    [[nodiscard]] std::optional<double> getNumber() const noexcept;
    [[nodiscard]] std::optional<std::string> getString() const noexcept;
    
    // Array operations
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] const JsonValue& operator[](std::size_t index) const;
    [[nodiscard]] JsonValue& operator[](std::size_t index);
    void push_back(JsonValue val);
    
    // Object operations
    [[nodiscard]] bool contains(const std::string& key) const noexcept;
    [[nodiscard]] const JsonValue& operator[](const std::string& key) const;
    [[nodiscard]] JsonValue& operator[](const std::string& key);
    [[nodiscard]] std::optional<std::reference_wrapper<const JsonValue>> get(const std::string& key) const noexcept;
    
    // Serialization
    [[nodiscard]] std::string dump(int indent = -1) const;
    
    // Comparison
    [[nodiscard]] bool operator==(const JsonValue& other) const;
    [[nodiscard]] bool operator!=(const JsonValue& other) const { return !(*this == other); }

private:
    ValueType value_;
    
    void dumpImpl(std::ostringstream& oss, int indent, int currentIndent) const;
};

// ============================================================================
// JSON Parser
// ============================================================================

/**
 * @brief JSON parse error
 */
class JsonParseError : public std::runtime_error {
public:
    JsonParseError(const std::string& msg, std::size_t pos)
        : std::runtime_error(msg + " at position " + std::to_string(pos))
        , position_(pos) {}
    
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

private:
    std::size_t position_;
};

/**
 * @brief JSON parser
 */
class JsonParser {
public:
    /**
     * @brief Parse JSON string
     * @param json JSON string
     * @return Parsed JSON value
     * @throws JsonParseError on invalid JSON
     */
    [[nodiscard]] static JsonValue parse(std::string_view json);
    
    /**
     * @brief Try to parse JSON string
     * @param json JSON string
     * @return Parsed value or nullopt on error
     */
    [[nodiscard]] static std::optional<JsonValue> tryParse(std::string_view json) noexcept;

private:
    std::string_view json_;
    std::size_t pos_ = 0;
    
    explicit JsonParser(std::string_view json) : json_(json) {}
    
    JsonValue parseValue();
    JsonValue parseNull();
    JsonValue parseBool();
    JsonValue parseNumber();
    JsonValue parseString();
    JsonValue parseArray();
    JsonValue parseObject();
    
    void skipWhitespace();
    char peek() const;
    char consume();
    bool match(char c);
    void expect(char c);
    std::string parseStringContent();
    char parseEscapeSequence();
};

// ============================================================================
// JSON Builder
// ============================================================================

/**
 * @brief Fluent JSON object builder
 */
class JsonObjectBuilder {
public:
    JsonObjectBuilder() = default;
    
    /**
     * @brief Add key-value pair
     */
    template<typename T>
    JsonObjectBuilder& add(const std::string& key, T&& value) {
        obj_[key] = JsonValue(std::forward<T>(value));
        return *this;
    }
    
    /**
     * @brief Add key with null value
     */
    JsonObjectBuilder& addNull(const std::string& key) {
        obj_[key] = JsonValue(nullptr);
        return *this;
    }
    
    /**
     * @brief Build the JSON object
     */
    [[nodiscard]] JsonValue build() {
        return JsonValue(std::move(obj_));
    }

private:
    JsonObject obj_;
};

/**
 * @brief Fluent JSON array builder
 */
class JsonArrayBuilder {
public:
    JsonArrayBuilder() = default;
    
    /**
     * @brief Add element
     */
    template<typename T>
    JsonArrayBuilder& add(T&& value) {
        arr_.push_back(JsonValue(std::forward<T>(value)));
        return *this;
    }
    
    /**
     * @brief Add null element
     */
    JsonArrayBuilder& addNull() {
        arr_.push_back(JsonValue(nullptr));
        return *this;
    }
    
    /**
     * @brief Build the JSON array
     */
    [[nodiscard]] JsonValue build() {
        return JsonValue(std::move(arr_));
    }

private:
    JsonArray arr_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Parse JSON from string
 */
[[nodiscard]] inline JsonValue parse(std::string_view json) {
    return JsonParser::parse(json);
}

/**
 * @brief Try to parse JSON from string
 */
[[nodiscard]] inline std::optional<JsonValue> tryParse(std::string_view json) noexcept {
    return JsonParser::tryParse(json);
}

/**
 * @brief Create JSON object
 */
[[nodiscard]] inline JsonObjectBuilder object() {
    return JsonObjectBuilder();
}

/**
 * @brief Create JSON array
 */
[[nodiscard]] inline JsonArrayBuilder array() {
    return JsonArrayBuilder();
}

// ============================================================================
// Implementation
// ============================================================================

inline JsonValue::JsonValue(std::initializer_list<JsonValue> init) {
    // Check if this looks like an object (pairs of string, value)
    if (init.size() % 2 == 0) {
        bool isObject = true;
        auto it = init.begin();
        while (it != init.end()) {
            if (!it->isString()) {
                isObject = false;
                break;
            }
            ++it;
            if (it != init.end()) ++it;
        }
        
        if (isObject && init.size() > 0) {
            JsonObject obj;
            it = init.begin();
            while (it != init.end()) {
                std::string key = it->asString();
                ++it;
                if (it != init.end()) {
                    obj[key] = *it;
                    ++it;
                }
            }
            value_ = std::move(obj);
            return;
        }
    }
    
    // Otherwise treat as array
    value_ = JsonArray(init);
}

inline JsonType JsonValue::type() const noexcept {
    return std::visit([](auto&& arg) -> JsonType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, JsonNull>) return JsonType::Null;
        else if constexpr (std::is_same_v<T, JsonBool>) return JsonType::Bool;
        else if constexpr (std::is_same_v<T, JsonNumber>) return JsonType::Number;
        else if constexpr (std::is_same_v<T, JsonString>) return JsonType::String;
        else if constexpr (std::is_same_v<T, JsonArray>) return JsonType::Array;
        else return JsonType::Object;
    }, value_);
}

inline bool JsonValue::asBool() const {
    if (auto* p = std::get_if<JsonBool>(&value_)) return *p;
    throw std::runtime_error("JSON value is not a boolean");
}

inline double JsonValue::asNumber() const {
    if (auto* p = std::get_if<JsonNumber>(&value_)) return *p;
    throw std::runtime_error("JSON value is not a number");
}

inline int JsonValue::asInt() const {
    return static_cast<int>(asNumber());
}

inline int64_t JsonValue::asInt64() const {
    return static_cast<int64_t>(asNumber());
}

inline const JsonString& JsonValue::asString() const {
    if (auto* p = std::get_if<JsonString>(&value_)) return *p;
    throw std::runtime_error("JSON value is not a string");
}

inline const JsonArray& JsonValue::asArray() const {
    if (auto* p = std::get_if<JsonArray>(&value_)) return *p;
    throw std::runtime_error("JSON value is not an array");
}

inline const JsonObject& JsonValue::asObject() const {
    if (auto* p = std::get_if<JsonObject>(&value_)) return *p;
    throw std::runtime_error("JSON value is not an object");
}

inline JsonArray& JsonValue::asArray() {
    if (auto* p = std::get_if<JsonArray>(&value_)) return *p;
    throw std::runtime_error("JSON value is not an array");
}

inline JsonObject& JsonValue::asObject() {
    if (auto* p = std::get_if<JsonObject>(&value_)) return *p;
    throw std::runtime_error("JSON value is not an object");
}

inline std::optional<bool> JsonValue::getBool() const noexcept {
    if (auto* p = std::get_if<JsonBool>(&value_)) return *p;
    return std::nullopt;
}

inline std::optional<double> JsonValue::getNumber() const noexcept {
    if (auto* p = std::get_if<JsonNumber>(&value_)) return *p;
    return std::nullopt;
}

inline std::optional<std::string> JsonValue::getString() const noexcept {
    if (auto* p = std::get_if<JsonString>(&value_)) return *p;
    return std::nullopt;
}

inline std::size_t JsonValue::size() const noexcept {
    if (auto* arr = std::get_if<JsonArray>(&value_)) return arr->size();
    if (auto* obj = std::get_if<JsonObject>(&value_)) return obj->size();
    return 0;
}

inline bool JsonValue::empty() const noexcept {
    if (auto* arr = std::get_if<JsonArray>(&value_)) return arr->empty();
    if (auto* obj = std::get_if<JsonObject>(&value_)) return obj->empty();
    return true;
}

inline const JsonValue& JsonValue::operator[](std::size_t index) const {
    return asArray().at(index);
}

inline JsonValue& JsonValue::operator[](std::size_t index) {
    return asArray().at(index);
}

inline void JsonValue::push_back(JsonValue val) {
    asArray().push_back(std::move(val));
}

inline bool JsonValue::contains(const std::string& key) const noexcept {
    if (auto* obj = std::get_if<JsonObject>(&value_)) {
        return obj->find(key) != obj->end();
    }
    return false;
}

inline const JsonValue& JsonValue::operator[](const std::string& key) const {
    const auto& obj = asObject();
    auto it = obj.find(key);
    if (it == obj.end()) {
        throw std::runtime_error("Key not found: " + key);
    }
    return it->second;
}

inline JsonValue& JsonValue::operator[](const std::string& key) {
    return asObject()[key];
}

inline std::optional<std::reference_wrapper<const JsonValue>> 
JsonValue::get(const std::string& key) const noexcept {
    if (auto* obj = std::get_if<JsonObject>(&value_)) {
        auto it = obj->find(key);
        if (it != obj->end()) {
            return std::cref(it->second);
        }
    }
    return std::nullopt;
}

inline bool JsonValue::operator==(const JsonValue& other) const {
    return value_ == other.value_;
}

inline std::string JsonValue::dump(int indent) const {
    std::ostringstream oss;
    dumpImpl(oss, indent, 0);
    return oss.str();
}

inline void JsonValue::dumpImpl(std::ostringstream& oss, int indent, int currentIndent) const {
    std::string indentStr = indent >= 0 ? std::string(currentIndent, ' ') : "";
    std::string newline = indent >= 0 ? "\n" : "";
    std::string space = indent >= 0 ? " " : "";
    
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, JsonNull>) {
            oss << "null";
        }
        else if constexpr (std::is_same_v<T, JsonBool>) {
            oss << (arg ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, JsonNumber>) {
            if (std::isnan(arg) || std::isinf(arg)) {
                oss << "null";
            } else if (arg == std::floor(arg) && std::abs(arg) < 1e15) {
                oss << static_cast<int64_t>(arg);
            } else {
                oss << std::setprecision(15) << arg;
            }
        }
        else if constexpr (std::is_same_v<T, JsonString>) {
            oss << '"';
            for (char c : arg) {
                switch (c) {
                    case '"': oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 32) {
                            oss << "\\u" << std::hex << std::setw(4) 
                                << std::setfill('0') << static_cast<int>(c);
                        } else {
                            oss << c;
                        }
                }
            }
            oss << '"';
        }
        else if constexpr (std::is_same_v<T, JsonArray>) {
            oss << "[";
            if (!arg.empty()) {
                oss << newline;
                for (std::size_t i = 0; i < arg.size(); ++i) {
                    if (indent >= 0) oss << std::string(currentIndent + indent, ' ');
                    arg[i].dumpImpl(oss, indent, currentIndent + indent);
                    if (i < arg.size() - 1) oss << ",";
                    oss << newline;
                }
                oss << indentStr;
            }
            oss << "]";
        }
        else if constexpr (std::is_same_v<T, JsonObject>) {
            oss << "{";
            if (!arg.empty()) {
                oss << newline;
                std::size_t i = 0;
                for (const auto& [key, val] : arg) {
                    if (indent >= 0) oss << std::string(currentIndent + indent, ' ');
                    oss << '"' << key << '"' << ":" << space;
                    val.dumpImpl(oss, indent, currentIndent + indent);
                    if (i < arg.size() - 1) oss << ",";
                    oss << newline;
                    ++i;
                }
                oss << indentStr;
            }
            oss << "}";
        }
    }, value_);
}

// Parser implementation
inline JsonValue JsonParser::parse(std::string_view json) {
    JsonParser parser(json);
    parser.skipWhitespace();
    JsonValue result = parser.parseValue();
    parser.skipWhitespace();
    if (parser.pos_ < parser.json_.size()) {
        throw JsonParseError("Unexpected characters after JSON", parser.pos_);
    }
    return result;
}

inline std::optional<JsonValue> JsonParser::tryParse(std::string_view json) noexcept {
    try {
        return parse(json);
    } catch (...) {
        return std::nullopt;
    }
}

inline JsonValue JsonParser::parseValue() {
    skipWhitespace();
    if (pos_ >= json_.size()) {
        throw JsonParseError("Unexpected end of input", pos_);
    }
    
    char c = peek();
    if (c == 'n') return parseNull();
    if (c == 't' || c == 'f') return parseBool();
    if (c == '"') return parseString();
    if (c == '[') return parseArray();
    if (c == '{') return parseObject();
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
    
    throw JsonParseError(std::string("Unexpected character: ") + c, pos_);
}

inline JsonValue JsonParser::parseNull() {
    if (json_.substr(pos_, 4) == "null") {
        pos_ += 4;
        return JsonValue(nullptr);
    }
    throw JsonParseError("Expected 'null'", pos_);
}

inline JsonValue JsonParser::parseBool() {
    if (json_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return JsonValue(true);
    }
    if (json_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return JsonValue(false);
    }
    throw JsonParseError("Expected 'true' or 'false'", pos_);
}

inline JsonValue JsonParser::parseNumber() {
    std::size_t start = pos_;
    
    if (peek() == '-') consume();
    
    if (peek() == '0') {
        consume();
    } else if (peek() >= '1' && peek() <= '9') {
        while (pos_ < json_.size() && peek() >= '0' && peek() <= '9') consume();
    } else {
        throw JsonParseError("Invalid number", pos_);
    }
    
    if (pos_ < json_.size() && peek() == '.') {
        consume();
        if (pos_ >= json_.size() || peek() < '0' || peek() > '9') {
            throw JsonParseError("Invalid number", pos_);
        }
        while (pos_ < json_.size() && peek() >= '0' && peek() <= '9') consume();
    }
    
    if (pos_ < json_.size() && (peek() == 'e' || peek() == 'E')) {
        consume();
        if (pos_ < json_.size() && (peek() == '+' || peek() == '-')) consume();
        if (pos_ >= json_.size() || peek() < '0' || peek() > '9') {
            throw JsonParseError("Invalid number", pos_);
        }
        while (pos_ < json_.size() && peek() >= '0' && peek() <= '9') consume();
    }
    
    std::string numStr(json_.substr(start, pos_ - start));
    return JsonValue(std::stod(numStr));
}

inline JsonValue JsonParser::parseString() {
    expect('"');
    std::string result = parseStringContent();
    expect('"');
    return JsonValue(std::move(result));
}

inline std::string JsonParser::parseStringContent() {
    std::string result;
    while (pos_ < json_.size() && peek() != '"') {
        if (peek() == '\\') {
            consume();
            result += parseEscapeSequence();
        } else {
            result += consume();
        }
    }
    return result;
}

inline char JsonParser::parseEscapeSequence() {
    if (pos_ >= json_.size()) {
        throw JsonParseError("Unexpected end of string", pos_);
    }
    
    char c = consume();
    switch (c) {
        case '"': return '"';
        case '\\': return '\\';
        case '/': return '/';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'u': {
            if (pos_ + 4 > json_.size()) {
                throw JsonParseError("Invalid unicode escape", pos_);
            }
            std::string hex(json_.substr(pos_, 4));
            pos_ += 4;
            int codePoint = std::stoi(hex, nullptr, 16);
            if (codePoint < 128) {
                return static_cast<char>(codePoint);
            }
            // For simplicity, just return '?' for non-ASCII unicode
            return '?';
        }
        default:
            throw JsonParseError(std::string("Invalid escape sequence: \\") + c, pos_);
    }
}

inline JsonValue JsonParser::parseArray() {
    expect('[');
    skipWhitespace();
    
    JsonArray arr;
    if (peek() != ']') {
        arr.push_back(parseValue());
        skipWhitespace();
        
        while (match(',')) {
            skipWhitespace();
            arr.push_back(parseValue());
            skipWhitespace();
        }
    }
    
    expect(']');
    return JsonValue(std::move(arr));
}

inline JsonValue JsonParser::parseObject() {
    expect('{');
    skipWhitespace();
    
    JsonObject obj;
    if (peek() != '}') {
        expect('"');
        std::string key = parseStringContent();
        expect('"');
        skipWhitespace();
        expect(':');
        skipWhitespace();
        obj[key] = parseValue();
        skipWhitespace();
        
        while (match(',')) {
            skipWhitespace();
            expect('"');
            key = parseStringContent();
            expect('"');
            skipWhitespace();
            expect(':');
            skipWhitespace();
            obj[key] = parseValue();
            skipWhitespace();
        }
    }
    
    expect('}');
    return JsonValue(std::move(obj));
}

inline void JsonParser::skipWhitespace() {
    while (pos_ < json_.size()) {
        char c = json_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++pos_;
        } else {
            break;
        }
    }
}

inline char JsonParser::peek() const {
    return pos_ < json_.size() ? json_[pos_] : '\0';
}

inline char JsonParser::consume() {
    return pos_ < json_.size() ? json_[pos_++] : '\0';
}

inline bool JsonParser::match(char c) {
    if (peek() == c) {
        ++pos_;
        return true;
    }
    return false;
}

inline void JsonParser::expect(char c) {
    if (!match(c)) {
        throw JsonParseError(std::string("Expected '") + c + "'", pos_);
    }
}

} // namespace json
} // namespace dfsmshsm

#endif // HSM_JSON_HPP
