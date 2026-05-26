#include "utils/json.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace webagent {

Json::Json() : value_(nullptr) {}
Json::Json(std::nullptr_t) : value_(nullptr) {}
Json::Json(const bool value) : value_(value) {}
Json::Json(const double value) : value_(value) {}
Json::Json(const int value) : value_(static_cast<double>(value)) {}
Json::Json(const char* value) : value_(std::string(value)) {}
Json::Json(std::string value) : value_(std::move(value)) {}
Json::Json(array value) : value_(std::move(value)) {}
Json::Json(object value) : value_(std::move(value)) {}

bool Json::isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
bool Json::isBool() const { return std::holds_alternative<bool>(value_); }
bool Json::isNumber() const { return std::holds_alternative<double>(value_); }
bool Json::isString() const { return std::holds_alternative<std::string>(value_); }
bool Json::isArray() const { return std::holds_alternative<array>(value_); }
bool Json::isObject() const { return std::holds_alternative<object>(value_); }

bool Json::asBool() const { return std::get<bool>(value_); }
double Json::asNumber() const { return std::get<double>(value_); }
const std::string& Json::asString() const { return std::get<std::string>(value_); }
const Json::array& Json::arrayItems() const { return std::get<array>(value_); }
const Json::object& Json::objectItems() const { return std::get<object>(value_); }

const Json* Json::find(const std::string& key) const {
    if (!isObject()) {
        return nullptr;
    }
    const auto& items = objectItems();
    const auto it = items.find(key);
    return it == items.end() ? nullptr : &it->second;
}

const Json& Json::at(const std::string& key) const {
    const auto* result = find(key);
    if (result == nullptr) {
        throw std::runtime_error("Missing JSON key: " + key);
    }
    return *result;
}

std::string Json::getString(const std::string& key, const std::string& fallback) const {
    const auto* value = find(key);
    return (value != nullptr && value->isString()) ? value->asString() : fallback;
}

double Json::getNumber(const std::string& key, const double fallback) const {
    const auto* value = find(key);
    return (value != nullptr && value->isNumber()) ? value->asNumber() : fallback;
}

bool Json::getBool(const std::string& key, const bool fallback) const {
    const auto* value = find(key);
    return (value != nullptr && value->isBool()) ? value->asBool() : fallback;
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& input) : input_(input) {}

    Json parseValue() {
        skipWhitespace();
        if (match("null")) {
            return Json(nullptr);
        }
        if (match("true")) {
            return Json(true);
        }
        if (match("false")) {
            return Json(false);
        }
        if (peek() == '"') {
            return Json(parseString());
        }
        if (peek() == '[') {
            return Json(parseArray());
        }
        if (peek() == '{') {
            return Json(parseObject());
        }
        if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            return Json(parseNumber());
        }
        throw std::runtime_error("Invalid JSON value");
    }

    void ensureDone() {
        skipWhitespace();
        if (position_ != input_.size()) {
            throw std::runtime_error("Unexpected trailing JSON content");
        }
    }

private:
    char peek() const {
        return position_ < input_.size() ? input_[position_] : '\0';
    }

    char consume() {
        if (position_ >= input_.size()) {
            throw std::runtime_error("Unexpected end of JSON");
        }
        return input_[position_++];
    }

    void skipWhitespace() {
        while (std::isspace(static_cast<unsigned char>(peek())) != 0) {
            ++position_;
        }
    }

    bool match(const char* token) {
        const std::string_view view(token);
        if (input_.substr(position_, view.size()) == view) {
            position_ += view.size();
            return true;
        }
        return false;
    }

    std::string parseString() {
        if (consume() != '"') {
            throw std::runtime_error("Expected JSON string");
        }

        std::string result;
        while (true) {
            const char ch = consume();
            if (ch == '"') {
                return result;
            }
            if (ch == '\\') {
                const char escaped = consume();
                switch (escaped) {
                    case '"':
                    case '\\':
                    case '/':
                        result += escaped;
                        break;
                    case 'b':
                        result += '\b';
                        break;
                    case 'f':
                        result += '\f';
                        break;
                    case 'n':
                        result += '\n';
                        break;
                    case 'r':
                        result += '\r';
                        break;
                    case 't':
                        result += '\t';
                        break;
                    default:
                        throw std::runtime_error("Unsupported JSON escape sequence");
                }
            } else {
                result += ch;
            }
        }
    }

    double parseNumber() {
        const std::size_t start = position_;
        if (peek() == '-') {
            ++position_;
        }
        while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            ++position_;
        }
        if (peek() == '.') {
            ++position_;
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                ++position_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++position_;
            if (peek() == '+' || peek() == '-') {
                ++position_;
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                ++position_;
            }
        }
        return std::stod(input_.substr(start, position_ - start));
    }

    Json::array parseArray() {
        consume();
        skipWhitespace();
        Json::array result;
        if (peek() == ']') {
            consume();
            return result;
        }

        while (true) {
            result.push_back(parseValue());
            skipWhitespace();
            const char ch = consume();
            if (ch == ']') {
                return result;
            }
            if (ch != ',') {
                throw std::runtime_error("Expected ',' or ']' in JSON array");
            }
            skipWhitespace();
        }
    }

    Json::object parseObject() {
        consume();
        skipWhitespace();
        Json::object result;
        if (peek() == '}') {
            consume();
            return result;
        }

        while (true) {
            const auto key = parseString();
            skipWhitespace();
            if (consume() != ':') {
                throw std::runtime_error("Expected ':' in JSON object");
            }
            skipWhitespace();
            result.emplace(key, parseValue());
            skipWhitespace();
            const char ch = consume();
            if (ch == '}') {
                return result;
            }
            if (ch != ',') {
                throw std::runtime_error("Expected ',' or '}' in JSON object");
            }
            skipWhitespace();
        }
    }

    const std::string& input_;
    std::size_t position_ = 0;
};

std::string escapeString(const std::string& value) {
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                output << ch;
                break;
        }
    }
    return output.str();
}

}  // namespace

std::string Json::stringify() const {
    if (isNull()) {
        return "null";
    }
    if (isBool()) {
        return asBool() ? "true" : "false";
    }
    if (isNumber()) {
        std::ostringstream output;
        output << std::setprecision(15) << asNumber();
        return output.str();
    }
    if (isString()) {
        return "\"" + escapeString(asString()) + "\"";
    }
    if (isArray()) {
        std::ostringstream output;
        output << "[";
        const auto& items = arrayItems();
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (index > 0) {
                output << ",";
            }
            output << items[index].stringify();
        }
        output << "]";
        return output.str();
    }

    std::ostringstream output;
    output << "{";
    bool first = true;
    for (const auto& [key, value] : objectItems()) {
        if (!first) {
            output << ",";
        }
        first = false;
        output << "\"" << escapeString(key) << "\":" << value.stringify();
    }
    output << "}";
    return output.str();
}

Json Json::parse(const std::string& input) {
    Parser parser(input);
    Json value = parser.parseValue();
    parser.ensureDone();
    return value;
}

}  // namespace webagent
