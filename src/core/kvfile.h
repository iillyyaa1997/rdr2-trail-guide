// key=value text files (UTF-8, '#' or ';' comments, [sections] are only headings for people and are ignored).
// Used for shipped data (TrailGuide.ini — read only) and for state files —
// written atomically with one .bak; keys the code does not know are kept as they were).
#pragma once
#include <map>
#include <string>
#include <vector>

namespace tg {

class KvFile {
public:
    bool load(const std::string& path);           // false if the file is missing or unreadable (contents cleared)
    void parse(const std::string& text);           // replaces contents
    std::string serialize() const;                 // key order: first appearance, new keys appended
    bool save(const std::string& path) const;      // atomic + .bak

    bool has(const std::string& key) const;
    std::string get(const std::string& key, const std::string& def = "") const;
    long long getInt(const std::string& key, long long def = 0) const;
    double getFloat(const std::string& key, double def = 0) const;
    bool getBool(const std::string& key, bool def = false) const;
    std::vector<std::string> getList(const std::string& key) const;      // comma separated
    std::vector<double> getFloats(const std::string& key) const;

    void set(const std::string& key, const std::string& value);
    void setInt(const std::string& key, long long value);
    void setFloat(const std::string& key, double value, int decimals = 3);
    void setBool(const std::string& key, bool value) { setInt(key, value ? 1 : 0); }
    void setList(const std::string& key, const std::vector<std::string>& values);
    void erase(const std::string& key);
    void clear();

    std::vector<std::string> keysWithPrefix(const std::string& prefix) const;
    const std::vector<std::string>& keys() const { return order_; }
    size_t size() const { return values_.size(); }

private:
    std::map<std::string, std::string> values_;
    std::vector<std::string> order_;
};

}  // namespace tg
