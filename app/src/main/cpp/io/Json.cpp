#include "io/Json.h"

#include <cstdlib>

namespace uvp {
namespace {

struct Parser {
    const char* p;
    const char* end;
    int depth = 0;

    static constexpr int kMaxDepth = 64;

    void skipWs() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }

    bool literal(const char* lit) {
        const char* q = p;
        while (*lit != '\0') {
            if (q >= end || *q != *lit) return false;
            ++q;
            ++lit;
        }
        p = q;
        return true;
    }

    bool parseString(std::string& out) {
        if (p >= end || *p != '"') return false;
        ++p;
        out.clear();
        while (p < end) {
            const char c = *p++;
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (p >= end) return false;
            const char esc = *p++;
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (end - p < 4) return false;
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = *p++;
                        cp <<= 4u;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else return false;
                    }
                    // Codificacion UTF-8 basica; los sustitutos se dejan tal cual
                    // porque glTF no los usa en las claves que nos interesan.
                    if (cp < 0x80) {
                        out.push_back(static_cast<char>(cp));
                    } else if (cp < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default:
                    return false;
            }
        }
        return false;
    }

    bool parseValue(JsonValue& out) {
        if (++depth > kMaxDepth) return false;
        const bool ok = parseValueInner(out);
        --depth;
        return ok;
    }

    bool parseValueInner(JsonValue& out) {
        skipWs();
        if (p >= end) return false;

        switch (*p) {
            case '{': {
                ++p;
                out.type = JsonValue::Type::Object;
                skipWs();
                if (p < end && *p == '}') { ++p; return true; }
                while (true) {
                    skipWs();
                    std::string key;
                    if (!parseString(key)) return false;
                    skipWs();
                    if (p >= end || *p != ':') return false;
                    ++p;
                    JsonValue child;
                    if (!parseValue(child)) return false;
                    out.fields.emplace_back(std::move(key), std::move(child));
                    skipWs();
                    if (p < end && *p == ',') { ++p; continue; }
                    if (p < end && *p == '}') { ++p; return true; }
                    return false;
                }
            }
            case '[': {
                ++p;
                out.type = JsonValue::Type::Array;
                skipWs();
                if (p < end && *p == ']') { ++p; return true; }
                while (true) {
                    JsonValue child;
                    if (!parseValue(child)) return false;
                    out.items.push_back(std::move(child));
                    skipWs();
                    if (p < end && *p == ',') { ++p; continue; }
                    if (p < end && *p == ']') { ++p; return true; }
                    return false;
                }
            }
            case '"':
                out.type = JsonValue::Type::String;
                return parseString(out.text);
            case 't':
                if (!literal("true")) return false;
                out.type = JsonValue::Type::Bool;
                out.boolean = true;
                return true;
            case 'f':
                if (!literal("false")) return false;
                out.type = JsonValue::Type::Bool;
                out.boolean = false;
                return true;
            case 'n':
                if (!literal("null")) return false;
                out.type = JsonValue::Type::Null;
                return true;
            default: {
                char* numEnd = nullptr;
                const double v = std::strtod(p, &numEnd);
                if (numEnd == p || numEnd > end) return false;
                p = numEnd;
                out.type = JsonValue::Type::Number;
                out.number = v;
                return true;
            }
        }
    }
};

}  // namespace

bool parseJson(const char* data, size_t length, JsonValue& out) {
    if (data == nullptr || length == 0) return false;
    Parser parser{data, data + length};
    if (!parser.parseValue(out)) return false;
    parser.skipWs();
    return true;
}

}  // namespace uvp
