#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mega::net {

/// Tiny read-only JSON value used to decode server snapshots. It supports the
/// subset the wire protocol needs (objects, arrays, numbers, strings, bools,
/// null) and never throws: malformed input simply yields a null value.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;

    static Json parse(const std::string& text);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    int asInt(int fallback = 0) const { return static_cast<int>(asNumber(fallback)); }
    float asFloat(float fallback = 0.0f) const { return static_cast<float>(asNumber(fallback)); }
    const std::string& asString() const { return string_; }

    /// Object member lookup; returns a null Json when absent.
    const Json& operator[](const std::string& key) const;

    const std::vector<Json>& items() const { return array_; }
    std::size_t size() const { return array_.size(); }

    /// Escapes `text` so it can be embedded in a JSON string literal.
    static std::string escape(const std::string& text);

private:
    static const Json& nullValue();
    static Json parseValue(const std::string& text, std::size_t& pos, bool& ok);

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    std::map<std::string, Json> object_;
};

}  // namespace mega::net
