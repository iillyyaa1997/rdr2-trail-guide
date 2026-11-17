// Entry point: settings, then input + autopilot every frame.
#include <string>

#include "core/fs.h"
#include "core/kvfile.h"
#include "core/log.h"
#include "core/version.h"
#include "mod/autopilot.h"
#include "mod/game.h"
#include "mod/input.h"
#include "mod/lang.h"
#include "mod/testhooks.h"
#include "sdk/natives.gen.h"

namespace tg {

static HMODULE g_module = nullptr;
static std::string g_dir;

namespace testhooks {
std::string asiDir() { return g_dir; }
}

static std::string moduleDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_module, buf, MAX_PATH);
    std::wstring w(buf, n);
    size_t cut = w.find_last_of(L"\\/");
    return fs::narrow(cut == std::wstring::npos ? L"." : w.substr(0, cut));
}

static void scriptMain() {
    g_dir = moduleDir();
    KvFile ini;
    bool haveIni = ini.load(fs::join(g_dir, "TrailGuide.ini"));
    log::init(fs::join(g_dir, "TrailGuide.log"), ini.getBool("DebugLog", false) ? log::Level::Debug : log::Level::Info);
    TG_INFO("Trail Guide %s%s, ini %s", TG_VERSION,
#ifdef TG_TEST
            " (test build)",
#else
            "",
#endif
            haveIni ? "loaded" : "missing (defaults)");
    if (!ini.getBool("Enabled", true)) {
        TG_INFO("Enabled=0: idle");
        return;
    }
    while (!PLAYER::IS_PLAYER_PLAYING(PLAYER::PLAYER_ID()) || CAMERA::IS_SCREEN_FADED_OUT()) scriptWait(500);
    scriptWait(2000);
    lang::init(LOCALIZATION::GET_CURRENT_LANGUAGE(), ini.get("Language", "auto"));
    TG_INFO("language %s", lang::russian() ? "ru" : "en");
    input::init(ini);
    autopilot::init(ini);
    while (true) {
        input::poll();
        autopilot::tick();
#ifdef TG_TEST
        testhooks::tick();
#endif
        scriptWait(0);
    }
}

}  // namespace tg

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        tg::g_module = module;
        scriptRegister(module, tg::scriptMain);
    } else if (reason == DLL_PROCESS_DETACH) {
        tg::input::shutdown();
        scriptUnregister(module);
    }
    return TRUE;
}
