#include "mega/net/Json.h"

#include <cstdlib>

namespace mega::net {
namespace {

void skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' ||
                                 text[pos] == '\r')) {
        ++pos;
    }
}

bool expect(const std::string& text, std::size_t& pos, char c) {
    skipWhitespace(text, pos);
    if (pos < text.size() && text[pos] == c) {
        ++pos;
        return true;
    }
    return false;
}

std::string parseString(const std::string& text, std::size_t& pos, bool& ok) {
    std::string out;
    if (!expect(text, pos, '"')) {
        ok = false;
        return out;
    }
    while (pos < text.size()) {
        const char c = text[pos++];
        if (c == '"') {
            return out;
        }
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (pos >= text.size()) {
            break;
        }
        const char esc = text[pos++];
        switch (esc) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'u': {
                if (pos + 4 > text.size()) {
                    ok = false;
                    return out;
                }
                const std::string hex = text.substr(pos, 4);
                pos += 4;
                const long code = std::strtol(hex.c_str(), nullptr, 16);
                // Only the BMP range that fits UTF-8 in up to 3 bytes is used
                // by the protocol (player names).
                if (code < 0x80) {
                    out.push_back(static_cast<char>(code));
                } else if (code < 0x800) {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                break;
            }
            default: out.push_back(esc); break;
        }
    }
    ok = false;
    return out;
}

}  // namespace

const Json& Json::nullValue() {
    static const Json value;
    return value;
}

bool Json::asBool(bool fallback) const {
    return type_ == Type::Bool ? bool_ : fallback;
}

double Json::asNumber(double fallback) const {
    return type_ == Type::Number ? number_ : fallback;
}

const Json& Json::operator[](const std::string& key) const {
    if (type_ != Type::Object) {
        return nullValue();
    }
    auto it = object_.find(key);
    return it == object_.end() ? nullValue() : it->second;
}

Json Json::parseValue(const std::string& text, std::size_t& pos, bool& ok) {
    Json value;
    skipWhitespace(text, pos);
    if (pos >= text.size()) {
        ok = false;
        return value;
    }

    const char c = text[pos];
    if (c == '{') {
        ++pos;
        value.type_ = Type::Object;
        skipWhitespace(text, pos);
        if (expect(text, pos, '}')) {
            return value;
        }
        while (ok) {
            skipWhitespace(text, pos);
            const std::string key = parseString(text, pos, ok);
            if (!ok || !expect(text, pos, ':')) {
                ok = false;
                break;
            }
            value.object_[key] = parseValue(text, pos, ok);
            if (expect(text, pos, ',')) {
                continue;
            }
            if (!expect(text, pos, '}')) {
                ok = false;
            }
            break;
        }
        return value;
    }
    if (c == '[') {
        ++pos;
        value.type_ = Type::Array;
        skipWhitespace(text, pos);
        if (expect(text, pos, ']')) {
            return value;
        }
        while (ok) {
            value.array_.push_back(parseValue(text, pos, ok));
            if (expect(text, pos, ',')) {
                continue;
            }
            if (!expect(text, pos, ']')) {
                ok = false;
            }
            break;
        }
        return value;
    }
    if (c == '"') {
        value.type_ = Type::String;
        value.string_ = parseString(text, pos, ok);
        return value;
    }
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        value.type_ = Type::Bool;
        value.bool_ = true;
        return value;
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        value.type_ = Type::Bool;
        value.bool_ = false;
        return value;
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        return value;
    }

    char* end = nullptr;
    value.number_ = std::strtod(text.c_str() + pos, &end);
    if (end == text.c_str() + pos) {
        ok = false;
        return value;
    }
    pos = static_cast<std::size_t>(end - text.c_str());
    value.type_ = Type::Number;
    return value;
}

Json Json::parse(const std::string& text) {
    std::size_t pos = 0;
    bool ok = true;
    Json value = parseValue(text, pos, ok);
    return ok ? value : Json{};
}

std::string Json::escape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char* kHex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4) & 0xF]);
                    out.push_back(kHex[c & 0xF]);
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

}  // namespace mega::net
