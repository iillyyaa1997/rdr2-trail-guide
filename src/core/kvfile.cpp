#include "core/kvfile.h"

#include <algorithm>
#include <cstdio>

#include "core/fs.h"
#include "core/strutil.h"

namespace tg {

bool KvFile::load(const std::string& path) {
    std::string text;
    if (!fs::readFile(path, text)) {
        clear();
        return false;
    }
    parse(text);
    return true;
}

void KvFile::parse(const std::string& text) {
    clear();
    size_t start = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        start = 3;  // UTF-8 BOM
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        std::string line = str::trim(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!line.empty() && line[0] != '#' && line[0] != ';' && line[0] != '[') {
            size_t eq = line.find('=');
            if (eq != std::string::npos && eq > 0) set(str::trim(line.substr(0, eq)), str::trim(line.substr(eq + 1)));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

std::string KvFile::serialize() const {
    std::string out;
    for (const auto& k : order_) {
        auto it = values_.find(k);
        if (it == values_.end()) continue;
        out += k;
        out += '=';
        out += it->second;
        out += "\r\n";
    }
    return out;
}

bool KvFile::save(const std::string& path) const { return fs::writeAtomic(path, serialize(), true); }

bool KvFile::has(const std::string& key) const { return values_.count(key) != 0; }

std::string KvFile::get(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    return it == values_.end() ? def : it->second;
}

long long KvFile::getInt(const std::string& key, long long def) const {
    long long v;
    return str::parseInt(get(key), v) ? v : def;
}

double KvFile::getFloat(const std::string& key, double def) const {
    double v;
    return str::parseFloat(get(key), v) ? v : def;
}

bool KvFile::getBool(const std::string& key, bool def) const {
    std::string v = str::lower(get(key));
    if (v.empty()) return def;
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return def;
}

std::vector<std::string> KvFile::getList(const std::string& key) const {
    std::vector<std::string> out;
    for (auto& s : str::split(get(key), ','))
        if (!s.empty()) out.push_back(s);
    return out;
}

std::vector<double> KvFile::getFloats(const std::string& key) const {
    std::vector<double> out;
    for (auto& s : getList(key)) {
        double v;
        if (str::parseFloat(s, v)) out.push_back(v);
    }
    return out;
}

void KvFile::set(const std::string& key, const std::string& value) {
    if (values_.find(key) == values_.end()) order_.push_back(key);
    values_[key] = value;
}

void KvFile::setInt(const std::string& key, long long value) { set(key, std::to_string(value)); }

void KvFile::setFloat(const std::string& key, double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, value);
    set(key, buf);
}

void KvFile::setList(const std::string& key, const std::vector<std::string>& values) {
    std::string joined;
    for (size_t i = 0; i < values.size(); ++i) joined += (i ? "," : "") + values[i];
    set(key, joined);
}

void KvFile::erase(const std::string& key) {
    if (values_.erase(key)) order_.erase(std::remove(order_.begin(), order_.end(), key), order_.end());
}

void KvFile::clear() {
    values_.clear();
    order_.clear();
}

std::vector<std::string> KvFile::keysWithPrefix(const std::string& prefix) const {
    std::vector<std::string> out;
    for (const auto& k : order_)
        if (str::startsWith(k, prefix)) out.push_back(k);
    return out;
}

}  // namespace tg
