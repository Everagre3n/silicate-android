// Tiny JSON reader/writer (no dependencies) so the generator core builds anywhere.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace silicate::json {

struct Value {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> a;
    std::vector<std::pair<std::string, Value>> o;

    static Value str(std::string v) { Value x; x.type = Str; x.s = std::move(v); return x; }
    static Value num(double v) { Value x; x.type = Num; x.n = v; return x; }
    static Value array() { Value x; x.type = Arr; return x; }
    static Value object() { Value x; x.type = Obj; return x; }

    Value& set(const std::string& k, Value v) { o.emplace_back(k, std::move(v)); return *this; }
    Value& push(Value v) { a.push_back(std::move(v)); return *this; }

    const Value* get(const std::string& k) const {
        if (type != Obj) return nullptr;
        for (auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    std::string getStr(const std::string& k, const std::string& d = "") const {
        auto v = get(k); return v && v->type == Str ? v->s : d;
    }
    double getNum(const std::string& k, double d = 0) const {
        auto v = get(k); return v && v->type == Num ? v->n : d;
    }
};

namespace detail {
struct Parser {
    const std::string& t; size_t i = 0;
    explicit Parser(const std::string& s) : t(s) {}
    [[noreturn]] void fail(const char* m) { throw std::runtime_error(std::string("json: ") + m); }
    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\t' || t[i] == '\r')) ++i; }
    bool lit(const char* w) {
        size_t n = std::string(w).size();
        if (t.compare(i, n, w) == 0) { i += n; return true; }
        return false;
    }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | cp >> 6); out += (char)(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | cp >> 12); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); }
        else { out += (char)(0xF0 | cp >> 18); out += (char)(0x80 | ((cp >> 12) & 63)); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); }
    }
    unsigned hex4() {
        if (i + 4 > t.size()) fail("bad escape");
        unsigned v = (unsigned)std::strtoul(t.substr(i, 4).c_str(), nullptr, 16);
        i += 4; return v;
    }
    std::string string() {
        std::string out; ++i;
        while (i < t.size() && t[i] != '"') {
            char c = t[i++];
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) fail("bad escape");
            char e = t[i++];
            switch (e) {
                case 'n': out += '\n'; break; case 't': out += '\t'; break;
                case 'r': out += '\r'; break; case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && t.compare(i, 2, "\\u") == 0) {
                        i += 2; unsigned lo = hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp); break;
                }
                default: out += e;
            }
        }
        if (i >= t.size()) fail("unterminated string");
        ++i; return out;
    }
    Value value(int depth = 0) {
        if (depth > 64) fail("too deep");
        ws();
        if (i >= t.size()) fail("unexpected end");
        char c = t[i];
        if (c == '{') {
            Value v = Value::object(); ++i; ws();
            if (t[i] == '}') { ++i; return v; }
            for (;;) {
                ws(); if (t[i] != '"') fail("expected key");
                std::string k = string(); ws();
                if (t[i++] != ':') fail("expected :");
                v.set(k, value(depth + 1)); ws();
                if (i >= t.size()) fail("unexpected end");
                if (t[i] == ',') { ++i; continue; }
                if (t[i] == '}') { ++i; return v; }
                fail("expected , or }");
            }
        }
        if (c == '[') {
            Value v = Value::array(); ++i; ws();
            if (t[i] == ']') { ++i; return v; }
            for (;;) {
                v.push(value(depth + 1)); ws();
                if (i >= t.size()) fail("unexpected end");
                if (t[i] == ',') { ++i; continue; }
                if (t[i] == ']') { ++i; return v; }
                fail("expected , or ]");
            }
        }
        if (c == '"') return Value::str(string());
        if (lit("true")) { Value v; v.type = Value::Bool; v.b = true; return v; }
        if (lit("false")) { Value v; v.type = Value::Bool; return v; }
        if (lit("null")) return Value();
        char* end = nullptr;
        double d = std::strtod(t.c_str() + i, &end);
        if (end == t.c_str() + i) fail("unexpected character");
        i = (size_t)(end - t.c_str());
        return Value::num(d);
    }
};
}  // namespace detail

inline Value parse(const std::string& s) {
    detail::Parser p(s);
    Value v = p.value();
    return v;
}

inline void dumpTo(const Value& v, std::string& out) {
    switch (v.type) {
        case Value::Null: out += "null"; break;
        case Value::Bool: out += v.b ? "true" : "false"; break;
        case Value::Num: {
            char buf[40]; std::snprintf(buf, sizeof buf, "%.10g", v.n); out += buf; break;
        }
        case Value::Str: {
            out += '"';
            for (unsigned char c : v.s) {
                switch (c) {
                    case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n"; break; case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                        else out += (char)c;
                }
            }
            out += '"'; break;
        }
        case Value::Arr:
            out += '[';
            for (size_t i = 0; i < v.a.size(); ++i) { if (i) out += ','; dumpTo(v.a[i], out); }
            out += ']'; break;
        case Value::Obj:
            out += '{';
            for (size_t i = 0; i < v.o.size(); ++i) {
                if (i) out += ',';
                dumpTo(Value::str(v.o[i].first), out); out += ':'; dumpTo(v.o[i].second, out);
            }
            out += '}'; break;
    }
}
inline std::string dump(const Value& v) { std::string s; dumpTo(v, s); return s; }

// Pull a JSON object out of text that may be wrapped in ``` fences or prose.
inline Value parseLoose(std::string t) {
    size_t i = t.find('{'), j = t.rfind('}');
    if (i == std::string::npos || j == std::string::npos || j < i) throw std::runtime_error("json: no object found");
    return parse(t.substr(i, j - i + 1));
}

}  // namespace silicate::json
