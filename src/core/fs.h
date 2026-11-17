// Small file helpers. Paths are UTF-8; on Windows they go through the wide APIs.
#pragma once
#include <string>

namespace tg::fs {

bool readFile(const std::string& path, std::string& out);
// Writes path.tmp, flushes, then replaces path in one step (a crash mid-write never leaves a half file).
// keepBackup: the previous file is copied to path.bak first.
bool writeAtomic(const std::string& path, const std::string& data, bool keepBackup);
bool exists(const std::string& path);
bool remove(const std::string& path);
bool copy(const std::string& from, const std::string& to);
bool makeDirs(const std::string& dir);
std::string join(const std::string& dir, const std::string& name);

#ifdef _WIN32
std::wstring widen(const std::string& utf8);
std::string narrow(const std::wstring& w);
#endif

}  // namespace tg::fs
