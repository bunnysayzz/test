#include "json.h"

#include <cmath>
#include <cstdlib>

namespace json {

const Value* Value::Find(const char* key) const {
    if (type != Type::Object || !key) return nullptr;
    for (const auto& entry : object)
        if (entry.first == key) return &entry.second;
    return nullptr;
}

const Value* Value::At(size_t index) const {
    if (type != Type::Array || index >= array.size()) return nullptr;
    return &array[index];
}

std::string Value::AsString() const {
    if (type == Type::String) return string;
    if (type == Type::Number) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%g", number);
        return buffer;
    }
    if (type == Type::Bool) return boolean ? "true" : "false";
    return {};
}

double Value::AsNumber(double fallback) const {
    return type == Type::Number ? number : fallback;
}

namespace {

constexpr int kMaxDepth = 64;

struct Parser {
    const char* p;
    const char* end;
    std::string error;
    int depth = 0;

    void Skip() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }

    bool Fail(const char* why) {
        if (error.empty()) error = why;
        return false;
    }

    bool Literal(const char* text) {
        const size_t length = std::char_traits<char>::length(text);
        if ((size_t)(end - p) < length || std::string(p, length) != text) return false;
        p += length;
        return true;
    }

    void AppendUtf8(std::string& out, uint32_t code) {
        if (code < 0x80) {
            out.push_back((char)code);
        } else if (code < 0x800) {
            out.push_back((char)(0xC0 | (code >> 6)));
            out.push_back((char)(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out.push_back((char)(0xE0 | (code >> 12)));
            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (code & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (code >> 18)));
            out.push_back((char)(0x80 | ((code >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (code & 0x3F)));
        }
    }

    bool Hex4(uint32_t& out) {
        if (end - p < 4) return Fail("short \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = *p++;
            out <<= 4;
            if (c >= '0' && c <= '9') out |= (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f') out |= (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= (uint32_t)(c - 'A' + 10);
            else return Fail("bad hex digit");
        }
        return true;
    }

    bool ParseString(std::string& out) {
        if (p >= end || *p != '"') return Fail("expected string");
        ++p;
        while (p < end) {
            const char c = *p++;
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (p >= end) return Fail("dangling escape");
            const char escape = *p++;
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t code = 0;
                    if (!Hex4(code)) return false;
                    if (code >= 0xD800 && code <= 0xDBFF && end - p >= 6 &&
                        p[0] == '\\' && p[1] == 'u') {
                        const char* save = p;
                        p += 2;
                        uint32_t low = 0;
                        if (Hex4(low) && low >= 0xDC00 && low <= 0xDFFF) {
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            p = save;
                        }
                    }
                    AppendUtf8(out, code);
                    break;
                }
                default: return Fail("unknown escape");
            }
        }
        return Fail("unterminated string");
    }

    bool ParseValue(Value& out) {
        if (++depth > kMaxDepth) return Fail("nesting too deep");
        Skip();
        if (p >= end) {
            --depth;
            return Fail("unexpected end");
        }
        bool ok = false;
        switch (*p) {
            case '{': ok = ParseObject(out); break;
            case '[': ok = ParseArray(out); break;
            case '"':
                out.type = Type::String;
                ok = ParseString(out.string);
                break;
            case 't':
                out.type = Type::Bool;
                out.boolean = true;
                ok = Literal("true");
                break;
            case 'f':
                out.type = Type::Bool;
                out.boolean = false;
                ok = Literal("false");
                break;
            case 'n':
                out.type = Type::Null;
                ok = Literal("null");
                break;
            default: {
                char* stop = nullptr;
                const double value = std::strtod(p, &stop);
                if (stop == p) return Fail("bad value");
                out.type = Type::Number;
                out.number = value;
                p = stop;
                ok = true;
                break;
            }
        }
        --depth;
        return ok;
    }

    bool ParseArray(Value& out) {
        out.type = Type::Array;
        ++p;
        Skip();
        if (p < end && *p == ']') {
            ++p;
            return true;
        }
        for (;;) {
            Value element;
            if (!ParseValue(element)) return false;
            out.array.push_back(std::move(element));
            Skip();
            if (p >= end) return Fail("unterminated array");
            if (*p == ',') {
                ++p;
                Skip();
                continue;
            }
            if (*p == ']') {
                ++p;
                return true;
            }
            return Fail("expected , or ]");
        }
    }

    bool ParseObject(Value& out) {
        out.type = Type::Object;
        ++p;
        Skip();
        if (p < end && *p == '}') {
            ++p;
            return true;
        }
        for (;;) {
            std::string key;
            Skip();
            if (!ParseString(key)) return false;
            Skip();
            if (p >= end || *p != ':') return Fail("expected :");
            ++p;
            Value element;
            if (!ParseValue(element)) return false;
            out.object.emplace_back(std::move(key), std::move(element));
            Skip();
            if (p >= end) return Fail("unterminated object");
            if (*p == ',') {
                ++p;
                continue;
            }
            if (*p == '}') {
                ++p;
                return true;
            }
            return Fail("expected , or }");
        }
    }
};

}  // namespace

bool Parse(const std::string& text, Value& out, std::string* error) {
    Parser parser{text.data(), text.data() + text.size(), {}, 0};
    out = Value{};
    if (!parser.ParseValue(out)) {
        if (error) *error = parser.error.empty() ? "parse failed" : parser.error;
        return false;
    }
    parser.Skip();
    if (parser.p != parser.end) {
        if (error) *error = "trailing data";
        return false;
    }
    return true;
}

std::string Quote(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    static const char digits[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(digits[c >> 4]);
                    out.push_back(digits[c & 15]);
                } else {
                    out.push_back((char)c);
                }
        }
    }
    out += "\"";
    return out;
}

}  // namespace json
