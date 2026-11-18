#include "mod/lang.h"

#include <cstring>

namespace tg::lang {

static bool g_ru = false;

struct Row {
    const char* id;
    const char* ru;
    const char* en;
};
static const Row kRows[] = {
    {"start", "Автопилот: к метке, {km} км · {speed}", "Autopilot: to the waypoint, {km} km · {speed}"},
    {"start_test", "Автопилот: к точке, {km} км · {speed}", "Autopilot: to the point, {km} km · {speed}"},
    {"no_waypoint", "Автопилот: сначала поставь метку на карте", "Autopilot: set a waypoint on the map first"},
    {"off", "Автопилот выключен", "Autopilot off"},
    {"takeover", "Автопилот: управление у тебя", "Autopilot: you have the reins"},
    {"arrived", "Автопилот: приехали", "Autopilot: arrived"},
    {"arrived_foot", "Автопилот: пришли", "Autopilot: arrived"},
    {"waypoint_gone", "Автопилот: метка снята", "Autopilot: the waypoint is gone"},
    {"speed", "Автопилот: {speed}", "Autopilot: {speed}"},
    {"predator", "Автопилот: рядом хищник", "Autopilot: a predator is near"},
    {"attack", "Автопилот: опасность!", "Autopilot: danger!"},
    {"hurt", "Автопилот: тебя ранили", "Autopilot: you are hurt"},
    {"horse_hurt", "Автопилот: конь ранен", "Autopilot: the horse is hurt"},
    {"thrown", "Автопилот: ты не в седле", "Autopilot: you are out of the saddle"},
    {"spooked", "Автопилот: конь испугался — поводья у тебя", "Autopilot: the horse got scared — you have the reins"},
    {"left_wagon", "Автопилот: ты сошёл с повозки", "Autopilot: you left the wagon"},
    {"stuck", "Автопилот: не могу пройти, дальше веди сам", "Autopilot: cannot get through, take over"},
    {"horse.0", "шагом", "walk"},
    {"horse.1", "рысью", "trot"},
    {"horse.2", "галопом", "gallop"},
    {"foot.0", "шагом", "walk"},
    {"foot.1", "бегом", "run"},
    {"foot.2", "во весь дух", "sprint"},
    {"wagon.0", "медленно", "slow"},
    {"wagon.1", "средне", "medium"},
    {"wagon.2", "быстро", "fast"},
};

void init(int gameLanguage, const std::string& setting) {
    if (setting == "ru") g_ru = true;
    else if (setting == "en") g_ru = false;
    else g_ru = gameLanguage == 7;  // the game's language list: en fr de it es pt-BR pl ru ...
}

bool russian() { return g_ru; }

const char* t(const char* id) {
    for (const Row& r : kRows)
        if (std::strcmp(r.id, id) == 0) return g_ru ? r.ru : r.en;
    return id;
}

}  // namespace tg::lang
