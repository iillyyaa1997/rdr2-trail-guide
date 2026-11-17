// The few texts the mod shows, in Russian and English (picked from the game's language, or Language= in the ini).
#pragma once
#include <string>

namespace tg::lang {

void init(int gameLanguage, const std::string& setting);  // setting: auto | ru | en
bool russian();
const char* t(const char* id);  // the id itself if unknown

}  // namespace tg::lang
