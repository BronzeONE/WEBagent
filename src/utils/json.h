#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace webagent {

class Json {
public:
    using object = std::map<std::string, Json>;
    using array = std::vector<Json>;

    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(double value);
    Json(int value);
    Json(const char* value);
    Json(std::string value);
    Json(array value);
    Json(object value);

    [[nodiscard]] bool isNull() const;
    [[nodiscard]] bool isBool() const;
    [[nodiscard]] bool isNumber() const;
    [[nodiscard]] bool isString() const;
    [[nodiscard]] bool isArray() const;
    [[nodiscard]] bool isObject() const;

    [[nodiscard]] bool asBool() const;
    [[nodiscard]] double asNumber() const;
    [[nodiscard]] const std::string& asString() const;
    [[nodiscard]] const array& arrayItems() const;
    [[nodiscard]] const object& objectItems() const;

    [[nodiscard]] const Json* find(const std::string& key) const;
    [[nodiscard]] const Json& at(const std::string& key) const;
    [[nodiscard]] std::string getString(const std::string& key, const std::string& fallback = "") const;
    [[nodiscard]] double getNumber(const std::string& key, double fallback = 0.0) const;
    [[nodiscard]] bool getBool(const std::string& key, bool fallback = false) const;

    [[nodiscard]] std::string stringify() const;

    static Json parse(const std::string& input);

private:
    using storage = std::variant<std::nullptr_t, bool, double, std::string, array, object>;

    storage value_;
};

}  // namespace webagent
