#ifdef TG_TEST
#include "mod/testhooks.h"

#include <cmath>
#include <string>
#include <vector>

#include "core/fs.h"
#include "core/log.h"
#include "core/strutil.h"
#include "mod/autopilot.h"
#include "mod/game.h"
#include "mod/input.h"
#include "sdk/natives.gen.h"

namespace tg::testhooks {

using namespace game;

std::string asiDir();  // main.cpp

static std::vector<Ped> g_spawned;
static std::vector<Vehicle> g_vehicles;
static int g_ctlUntil = 0, g_ctlNext = 0;

static void run(const std::vector<std::string>& a) {
    const std::string& cmd = a[0];
    auto num = [&](size_t i, double def) { double v; return i < a.size() && str::parseFloat(a[i], v) ? v : def; };
    Ped pl = player();
    Vector3 p = pos(pl);
    float hd = ENTITY::GET_ENTITY_HEADING(pl) * 3.14159265f / 180.0f;
    Vector3 fwd(-std::sin(hd), std::cos(hd), 0);
    if (cmd == "go" && a.size() >= 3) {
        autopilot::startTo(Vector3(float(num(1, 0)), float(num(2, 0)), p.z));
    } else if (cmd == "ahead") {
        // ahead <metres> [degrees to the right]
        float m = float(num(1, 100)), deg = float(num(2, 0)) * 3.14159265f / 180.0f;
        Vector3 d(fwd.x * std::cos(deg) + fwd.y * std::sin(deg), fwd.y * std::cos(deg) - fwd.x * std::sin(deg), 0);
        autopilot::startTo(add(p, scale(d, m)));
    } else if (cmd == "stop") {
        autopilot::stopNow();
    } else if (cmd == "method" && a.size() >= 3) {
        autopilot::Mode m = a[1] == "foot" ? autopilot::Mode::Foot : a[1] == "wagon" ? autopilot::Mode::Wagon : autopilot::Mode::Horse;
        autopilot::setMethod(m, int(num(2, 0)));
        TG_INFO("test: method %s %d", a[1].c_str(), int(num(2, 0)));
    } else if (cmd == "step") {
        autopilot::setStep(int(num(1, 2)));
    } else if (cmd == "log") {
        autopilot::logFor(int(num(1, 30)));
    } else if (cmd == "toggle") {
        input::fakeToggle();
    } else if (cmd == "cycle") {
        input::fakeSpeed();
    } else if (cmd == "state") {
        TG_INFO("test: %s", autopilot::state().c_str());
    } else if (cmd == "ctl") {
        g_ctlUntil = now() + int(num(1, 15) * 1000);
    } else if (cmd == "wp") {
        Vector3 w = MAP::_GET_WAYPOINT_COORDS();
        TG_INFO("test: waypoint active %d at %.1f %.1f %.1f, %.0f m", int(MAP::IS_WAYPOINT_ACTIVE()), w.x, w.y, w.z, dist2d(w, p));
    } else if (cmd == "pos") {
        TG_INFO("test: Arthur %.2f %.2f %.2f heading %.0f, mounted %d, in vehicle %d, health %d", p.x, p.y, p.z,
                ENTITY::GET_ENTITY_HEADING(pl), int(PED::IS_PED_ON_MOUNT(pl)), int(PED::IS_PED_IN_ANY_VEHICLE(pl, false)),
                ENTITY::GET_ENTITY_HEALTH(pl));
    } else if (cmd == "tpg" && a.size() >= 3) {
        // tpg <x> <y> [horse]: Arthur to the ground at x,y; 'horse' brings the saddle horse beside him
        float x = float(num(1, 0)), y = float(num(2, 0)), z = 0;
        ENTITY::SET_ENTITY_COORDS(pl, x, y, 1000.0f, false, false, false, true);
        ENTITY::FREEZE_ENTITY_POSITION(pl, true);
        bool ok = groundZ(x, y, z, 5000);
        ENTITY::FREEZE_ENTITY_POSITION(pl, false);
        if (!ok) z = 120.0f;
        ENTITY::SET_ENTITY_COORDS(pl, x, y, z + 0.5f, false, false, false, true);
        TG_INFO("test: tpg %.1f %.1f -> ground %.2f (%s)", x, y, z, ok ? "found" : "NOT found");
        Ped horse = PLAYER::_GET_SADDLE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());
        if (a.size() > 3 && a[3] == "horse" && alive(horse))
            ENTITY::SET_ENTITY_COORDS(horse, x + 3, y, z + 0.5f, false, false, false, true);
    } else if (cmd == "mount") {
        Ped horse = PLAYER::_GET_SADDLE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());
        if (alive(horse)) {
            if (dist(pos(horse), p) > 6) ENTITY::SET_ENTITY_COORDS(horse, p.x + 3, p.y, p.z, false, false, false, true);
            PED::SET_PED_ONTO_MOUNT(pl, horse, -1, true);
        }
        TG_INFO("test: mount horse %d -> mounted %d", horse, int(PED::IS_PED_ON_MOUNT(pl)));
    } else if (cmd == "dismount") {
        TASK::TASK_DISMOUNT_ANIMAL(pl, 0, 0, 0, 0, 0);
    } else if (cmd == "wolf" || cmd == "cougar" || cmd == "bandit") {
        const char* model = cmd == "wolf" ? "A_C_Wolf" : cmd == "cougar" ? "A_C_Cougar_01" : "G_M_M_UniBanditos_01";
        Ped s = spawnPed(model, add(p, scale(fwd, float(num(1, 25)))), 0);
        if (s) {
            g_spawned.push_back(s);
            if (cmd == "bandit") {
                WEAPON::GIVE_WEAPON_TO_PED(s, MISC::GET_HASH_KEY("WEAPON_REVOLVER_CATTLEMAN"), 50, true, false, 0, false, 0.5f, 1.0f, 0, false, 0.0f, false);
                TASK::TASK_COMBAT_PED(s, pl, 0, 16);
            }
        }
        TG_INFO("test: %s %d at %.0f m", model, s, num(1, 25));
    } else if (cmd == "clean") {
        for (Ped& s : g_spawned)
            if (exists(s)) PED::DELETE_PED(&s);
        g_spawned.clear();
        for (Vehicle& v : g_vehicles)
            if (exists(v)) VEHICLE::DELETE_VEHICLE(&v);
        g_vehicles.clear();
    } else if (cmd == "wagon") {
        const char* model = a.size() > 1 ? a[1].c_str() : "CART01";
        Hash h = MISC::GET_HASH_KEY(model);
        if (loadModel(h, 5000)) {
            Vector3 at = add(p, scale(fwd, 6));
            Vehicle v = VEHICLE::CREATE_VEHICLE(h, at.x, at.y, at.z, ENTITY::GET_ENTITY_HEADING(pl), false, false, false, false);
            STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(h);
            if (v) {
                g_vehicles.push_back(v);
                PED::SET_PED_INTO_VEHICLE(pl, v, -1);
            }
            TG_INFO("test: wagon %s %d, driver %d", model, v, v ? int(VEHICLE::GET_PED_IN_VEHICLE_SEAT(v, -1) == pl) : 0);
        } else {
            TG_INFO("test: wagon model %s did not load", model);
        }
    } else if (cmd == "cine") {
        CAMERA::SET_CINEMATIC_MODE_ACTIVE(num(1, 1) != 0);
        TG_INFO("test: cinematic %d -> in cinematic %d", int(num(1, 1)), int(CAMERA::_IS_IN_CINEMATIC_MODE()));
    } else if (cmd == "time") {
        CLOCK::SET_CLOCK_TIME(int(num(1, 12)), 0, 0);
    } else if (cmd == "heal") {
        ENTITY::SET_ENTITY_HEALTH(pl, ENTITY::GET_ENTITY_MAX_HEALTH(pl, false), 0);
    } else {
        TG_INFO("test: unknown '%s'", cmd.c_str());
    }
}

void tick() {
    if (now() < g_ctlUntil && now() >= g_ctlNext) {
        g_ctlNext = now() + 500;
        TG_INFO("ctl: %s", input::describe().c_str());
    }
    static int next = 0;
    if (now() < next) return;
    next = now() + 1000;
    const std::string path = fs::join(asiDir(), "TrailGuide.test");
    std::string text;
    if (!fs::readFile(path, text)) return;
    fs::remove(path);
    for (auto& line : str::split(text, '\n')) {
        std::vector<std::string> args;
        for (auto& x : str::split(str::trim(line), ' '))
            if (!x.empty()) args.push_back(x);
        if (!args.empty()) {
            TG_INFO("test: > %s", str::trim(line).c_str());
            run(args);
        }
    }
}

}  // namespace tg::testhooks
#endif
