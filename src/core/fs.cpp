#include "core/fs.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tg::fs {

#ifdef _WIN32
std::wstring widen(const std::string& utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
#endif

bool readFile(const std::string& path, std::string& out) {
#ifdef _WIN32
    FILE* f = _wfopen(widen(path).c_str(), L"rb");
#else
    FILE* f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) return false;
    out.clear();
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

static bool writeRaw(const std::string& path, const std::string& data) {
#ifdef _WIN32
    FILE* f = _wfopen(widen(path).c_str(), L"wb");
#else
    FILE* f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (std::fflush(f) == 0) && ok;
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

bool exists(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesW(widen(path).c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0;
#endif
}

bool remove(const std::string& path) {
#ifdef _WIN32
    return DeleteFileW(widen(path).c_str()) != 0;
#else
    return ::unlink(path.c_str()) == 0;
#endif
}

bool copy(const std::string& from, const std::string& to) {
    std::string data;
    return readFile(from, data) && writeRaw(to, data);
}

bool makeDirs(const std::string& dir) {
    if (dir.empty() || exists(dir)) return true;
    size_t cut = dir.find_last_of("/\\");
    if (cut != std::string::npos && cut > 0 && !makeDirs(dir.substr(0, cut))) return false;
#ifdef _WIN32
    return CreateDirectoryW(widen(dir).c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return ::mkdir(dir.c_str(), 0755) == 0 || exists(dir);
#endif
}

bool writeAtomic(const std::string& path, const std::string& data, bool keepBackup) {
    const std::string tmp = path + ".tmp";
    if (!writeRaw(tmp, data)) return false;
    if (keepBackup && exists(path)) copy(path, path + ".bak");
#ifdef _WIN32
    if (!MoveFileExW(widen(tmp).c_str(), widen(path).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        remove(tmp);
        return false;
    }
    return true;
#else
    return std::rename(tmp.c_str(), path.c_str()) == 0;
#endif
}

std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    char last = dir.back();
    return (last == '/' || last == '\\') ? dir + name : dir + "\\" + name;
}

}  // namespace tg::fs
