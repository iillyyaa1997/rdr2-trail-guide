#include "mod/input.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "core/strutil.h"
#include "mod/game.h"
#include "sdk/natives.gen.h"

namespace tg::input {

// XInput types (declared here so the build does not depend on the SDK header's casing)
struct XGamepad {
    WORD buttons;
    BYTE leftTrigger, rightTrigger;
    SHORT thumbLX, thumbLY, thumbRX, thumbRY;
};
struct XState {
    DWORD packet;
    XGamepad pad;
};
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XState*);

static HMODULE g_xinput = nullptr;
static XInputGetStateFn g_getState = nullptr;  // the real pad (xinput1_4 / 1_3 / 9_1_0 loaded by us, never patched)

// ---------------------------------------------------------------- pad override (see input.h)
static volatile LONG g_ovActive = 0;
static volatile LONG g_ovSeq = 0;
static volatile SHORT g_ovLX = 0, g_ovLY = 0;
static volatile WORD g_ovButtons = 0;
static std::string g_hookInfo = "not installed";

static DWORD WINAPI hookedGetState(DWORD index, XState* s) {
    if (!g_getState) return ERROR_DEVICE_NOT_CONNECTED;
    DWORD r = g_getState(index, s);
    if (r == 0 && g_ovActive && s) {
        s->pad.thumbLX = g_ovLX;
        s->pad.thumbLY = g_ovLY;
        s->pad.buttons |= g_ovButtons;
        s->packet += 0x40000000u + DWORD(g_ovSeq);  // a new packet whenever our values change: the game re-reads it
    }
    return r;
}

static bool patchExport(HMODULE m, const char* dll) {
    BYTE* f = (BYTE*)GetProcAddress(m, "XInputGetState");
    if (!f) return false;
    char bytes[64];
    std::snprintf(bytes, sizeof bytes, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", f[0], f[1], f[2], f[3],
                  f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11]);
    // 12 bytes: mov rax, imm64 ; jmp rax
    BYTE code[12] = {0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0};
    void* target = (void*)&hookedGetState;
    std::memcpy(code + 2, &target, 8);
    DWORD old = 0;
    if (!VirtualProtect(f, sizeof code, PAGE_EXECUTE_READWRITE, &old)) {
        TG_WARN("pad hook: %s VirtualProtect failed (%lu)", dll, GetLastError());
        return false;
    }
    std::memcpy(f, code, sizeof code);
    VirtualProtect(f, sizeof code, old, &old);
    FlushInstructionCache(GetCurrentProcess(), f, sizeof code);
    TG_INFO("pad hook: %s XInputGetState patched (was %s)", dll, bytes);
    return true;
}

bool padHookInstall() {
    if (!g_getState) {
        g_hookInfo = "no real XInput to read the pad";
        return false;
    }
    int n = 0;
    std::string which;
    // the game's XInput DLLs; xinput1_4 is ours (raw reads) unless the game loaded only that one
    for (const wchar_t* dll : {L"xinput9_1_0.dll", L"xinput1_3.dll", L"xinput1_2.dll", L"xinput1_1.dll"}) {
        HMODULE m = GetModuleHandleW(dll);
        if (!m || m == g_xinput) continue;
        char name[32];
        std::snprintf(name, sizeof name, "%ls", dll);
        if (patchExport(m, name)) { ++n; which += std::string(which.empty() ? "" : ", ") + name; }
    }
    g_hookInfo = n ? "patched " + which : "no game XInput DLL found";
    if (!n) TG_WARN("pad hook: %s", g_hookInfo.c_str());
    return n > 0;
}

void padOverride(float lx, float ly, unsigned short buttons) {
    auto toShort = [](float v) { v = v < -1 ? -1 : (v > 1 ? 1 : v); return SHORT(v * 32767.0f); };
    SHORT nx = toShort(lx), ny = toShort(ly);
    if (!g_ovActive || nx != g_ovLX || ny != g_ovLY || buttons != g_ovButtons) InterlockedIncrement(&g_ovSeq);
    g_ovLX = nx;
    g_ovLY = ny;
    g_ovButtons = buttons;
    InterlockedExchange(&g_ovActive, 1);
}

void padRelease() {
    if (g_ovActive) InterlockedIncrement(&g_ovSeq);
    InterlockedExchange(&g_ovActive, 0);
}

std::string padHookInfo() { return g_hookInfo; }

struct Button {
    WORD mask = 0;
    int holdMs = 0;
    bool down = false, fired = false;
    int downAt = 0;
};
static Button g_toggle, g_speed;
static DWORD g_toggleKey = 0, g_speedKey = 0;
static volatile LONG g_keyToggle = 0, g_keySpeed = 0;  // set by the key handler, taken by poll()
static bool g_toggleNow = false, g_speedNow = false;
static bool g_fakeToggle = false, g_fakeSpeed = false;
static float g_move = 0;
static WORD g_buttons = 0;
static float g_stick = 0;
static bool g_connected = false;
static Hash g_moveControls[6] = {};
static float g_deadzone = 0.5f;
static int g_moveKeys[8] = {};
static int g_moveKeyCount = 0;

// the game window has the focus (keyboard state of other windows must not stop the autopilot)
static bool gameFocused() {
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static WORD buttonBit(const std::string& n) {
    static const struct { const char* name; WORD bit; } kNames[] = {
        {"DPAD_UP", 0x0001}, {"DPAD_DOWN", 0x0002}, {"DPAD_LEFT", 0x0004}, {"DPAD_RIGHT", 0x0008},
        {"START", 0x0010}, {"OPTIONS", 0x0010}, {"BACK", 0x0020}, {"SHARE", 0x0020}, {"L3", 0x0040},
        {"LTHUMB", 0x0040}, {"R3", 0x0080}, {"RTHUMB", 0x0080}, {"LB", 0x0100}, {"L1", 0x0100}, {"RB", 0x0200},
        {"R1", 0x0200}, {"A", 0x1000}, {"CROSS", 0x1000}, {"B", 0x2000}, {"CIRCLE", 0x2000}, {"X", 0x4000},
        {"SQUARE", 0x4000}, {"Y", 0x8000}, {"TRIANGLE", 0x8000}};
    for (auto& k : kNames)
        if (n == k.name) return k.bit;
    return 0;
}

// "DPAD_LEFT" or "L1+DPAD_LEFT": every named button must be down
static WORD parseButtons(const std::string& spec) {
    WORD m = 0;
    for (auto& part : str::split(spec, '+')) {
        std::string u = part;
        for (char& c : u) if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        WORD b = buttonBit(u);
        if (!b && !u.empty() && u != "NONE") TG_WARN("unknown gamepad button '%s'", part.c_str());
        m |= b;
    }
    return m;
}

// "K", "F9", "0x4B"
static DWORD parseKey(const std::string& spec) {
    std::string u = str::trim(spec);
    for (char& c : u) if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    if (u.empty() || u == "NONE") return 0;
    if (u.size() == 1 && ((u[0] >= 'A' && u[0] <= 'Z') || (u[0] >= '0' && u[0] <= '9'))) return DWORD(u[0]);
    if (u[0] == 'F' && u.size() <= 3) {
        int n = std::atoi(u.c_str() + 1);
        if (n >= 1 && n <= 24) return DWORD(VK_F1 + n - 1);
    }
    long long v;
    if (str::parseInt(u, v)) return DWORD(v);
    TG_WARN("unknown key '%s'", spec.c_str());
    return 0;
}

static void onKey(DWORD key, WORD, BYTE, BOOL, BOOL, BOOL wasDownBefore, BOOL isUpNow) {
    if (isUpNow || wasDownBefore) return;
    if (key == g_toggleKey && g_toggleKey) InterlockedExchange(&g_keyToggle, 1);
    if (key == g_speedKey && g_speedKey) InterlockedExchange(&g_keySpeed, 1);
}

void init(const KvFile& ini) {
    g_toggle.mask = parseButtons(ini.get("ToggleButton", "DPAD_LEFT"));
    g_toggle.holdMs = int(ini.getInt("ToggleHoldMs", 0));
    g_speed.mask = parseButtons(ini.get("SpeedButton", "DPAD_RIGHT"));
    g_toggleKey = parseKey(ini.get("ToggleKey", "K"));
    g_speedKey = parseKey(ini.get("SpeedKey", "L"));
    g_deadzone = float(ini.getFloat("TakeoverStick", 0.5));
    g_moveKeyCount = 0;
    for (auto& k : str::split(ini.get("TakeoverKeys", "W,A,S,D"), ','))
        if (DWORD vk = parseKey(k); vk && g_moveKeyCount < 8) g_moveKeys[g_moveKeyCount++] = int(vk);
    for (const wchar_t* dll : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
        g_xinput = LoadLibraryW(dll);
        if (g_xinput) {
            g_getState = (XInputGetStateFn)GetProcAddress(g_xinput, "XInputGetState");
            if (g_getState) break;
            FreeLibrary(g_xinput);
            g_xinput = nullptr;
        }
    }
    keyboardHandlerRegister(onKey);
    const char* names[6] = {"INPUT_MOVE_LR", "INPUT_MOVE_UD", "INPUT_HORSE_MOVE_LR", "INPUT_HORSE_MOVE_UD",
                            "INPUT_VEH_MOVE_LR", "INPUT_VEH_MOVE_UD"};
    for (int i = 0; i < 6; ++i) g_moveControls[i] = game::hash(names[i]);
    TG_INFO("input: pad toggle 0x%04X (hold %d ms), speed 0x%04X; keys toggle 0x%02X speed 0x%02X; xinput %s",
            g_toggle.mask, g_toggle.holdMs, g_speed.mask, g_toggleKey, g_speedKey, g_getState ? "loaded" : "MISSING");
}

void shutdown() { keyboardHandlerUnregister(onKey); }

static bool step(Button& b, WORD buttons, int t) {
    if (!b.mask) return false;
    bool down = (buttons & b.mask) == b.mask;
    bool fire = false;
    if (down && !b.down) { b.downAt = t; b.fired = false; }
    if (down && !b.fired && t - b.downAt >= b.holdMs) { fire = true; b.fired = true; }
    b.down = down;
    return fire;
}

void poll() {
    int t = game::now();
    g_buttons = 0;
    g_stick = 0;
    g_connected = false;
    if (g_getState) {
        for (DWORD i = 0; i < 4; ++i) {
            XState s{};
            if (g_getState(i, &s) != 0) continue;  // ERROR_DEVICE_NOT_CONNECTED
            g_connected = true;
            g_buttons |= s.pad.buttons;
            float lx = std::fabs(s.pad.thumbLX / 32767.0f), ly = std::fabs(s.pad.thumbLY / 32767.0f);
            if (lx > g_stick) g_stick = lx;
            if (ly > g_stick) g_stick = ly;
        }
    }
    g_toggleNow = step(g_toggle, g_buttons, t);
    g_speedNow = step(g_speed, g_buttons, t);
    if (InterlockedExchange(&g_keyToggle, 0)) g_toggleNow = true;
    if (InterlockedExchange(&g_keySpeed, 0)) g_speedNow = true;
    if (g_fakeToggle) { g_toggleNow = true; g_fakeToggle = false; }
    if (g_fakeSpeed) { g_speedNow = true; g_fakeSpeed = false; }
    g_move = g_stick;
    if (gameFocused())
        for (int i = 0; i < g_moveKeyCount; ++i)
            if (GetAsyncKeyState(g_moveKeys[i]) & 0x8000) g_move = 1.0f;
}

bool togglePressed() { return g_toggleNow; }
bool speedPressed() { return g_speedNow; }
float moveAmount() { return g_move; }
bool padConnected() { return g_connected; }
void fakeToggle() { g_fakeToggle = true; }
void fakeSpeed() { g_fakeSpeed = true; }

std::string describe() {
    char buf[200];
    float c[6];
    for (int i = 0; i < 6; ++i) c[i] = PAD::GET_DISABLED_CONTROL_NORMAL(0, g_moveControls[i]);
    std::snprintf(buf, sizeof buf, "pad %s buttons 0x%04X stick %.2f | move %.2f %.2f horse %.2f %.2f veh %.2f %.2f",
                  g_connected ? "on" : "off", g_buttons, g_stick, c[0], c[1], c[2], c[3], c[4], c[5]);
    return buf;
}

}  // namespace tg::input
