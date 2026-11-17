// String helpers shared by the data, ini and language code.
#pragma once
#include <cstdlib>
#include <string>
#include <vector>

namespace tg::str {

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    if (s.empty()) return out;
    size_t start = 0;
    while (true) {
        size_t p = s.find(sep, start);
        out.push_back(trim(s.substr(start, p == std::string::npos ? std::string::npos : p - start)));
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return out;
}

inline std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

inline bool parseInt(const std::string& s, long long& out) {
    std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    int base = (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) ? 16 : 10;
    long long v = std::strtoll(t.c_str(), &end, base);
    if (base == 16 && v == 0 && t.size() > 2) v = (long long)std::strtoull(t.c_str(), &end, 16);
    if (!end || *end != '\0') return false;
    out = v;
    return true;
}

inline bool parseFloat(const std::string& s, double& out) {
    std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);
    if (!end || *end != '\0') return false;
    out = v;
    return true;
}

// Replaces {key} placeholders: format("{name} sits", {{"name","Мики"}}).
inline std::string format(std::string text, const std::vector<std::pair<std::string, std::string>>& args) {
    for (const auto& [k, v] : args) {
        const std::string tag = "{" + k + "}";
        for (size_t p = text.find(tag); p != std::string::npos; p = text.find(tag, p + v.size())) text.replace(p, tag.size(), v);
    }
    return text;
}

// joaat — the game's Jenkins one-at-a-time hash of a lower-cased string (model names, events, prompts).
inline unsigned joaat(const char* s) {
    unsigned h = 0;
    for (; *s; ++s) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        h += (unsigned char)c;
        h += h << 10;
        h ^= h >> 6;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}

}  // namespace tg::str
