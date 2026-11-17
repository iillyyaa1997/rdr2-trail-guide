#include "mod/autopilot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/log.h"
#include "core/strutil.h"
#include "mod/game.h"
#include "mod/input.h"
#include "mod/lang.h"
#include "sdk/natives.gen.h"

namespace tg::autopilot {

using namespace game;

// ------------------------------------------------------------------------------------------------ settings
struct Settings {
    float speed[4][3] = {};      // [mode][step]: move blend ratio (horse, foot) or m/s (wagon)
    int step[4] = {0, 1, 1, 1};  // starting speed per mode
    float arrive[4] = {0, 12, 3, 15};
    int method[4] = {0, 0, 0, 0};
    float legLength = 0;         // > 0: travel in legs of this length (snapped to the nearest road when RoadSnap=1)
    bool roadSnap = true;
    bool stopOnDanger = true, stopOnTakeover = true, cinematic = false, clearWaypoint = true;
    float predatorRange = 35, threatRange = 60, takeover = 0.5f;
    int takeoverMs = 250, damage = 3, noticeMs = 3000, stuckMs = 9000, stuckTries = 4;
    int wagonFlags = 0;
};
static Settings S;
static int g_stepNow[4] = {0, 1, 1, 1};  // current speed per mode (a change stays until the game restarts)

static void loadSpeeds(const KvFile& ini, const char* key, Mode m, const char* def) {
    std::vector<double> v = ini.getFloats(key);
    if (v.size() != 3) {
        KvFile d;
        d.parse(std::string("x=") + def);
        v = d.getFloats("x");
    }
    for (int i = 0; i < 3; ++i) S.speed[int(m)][i] = float(v[i]);
}

void init(const KvFile& ini) {
    loadSpeeds(ini, "HorseSpeeds", Mode::Horse, "1.0,2.0,3.0");
    loadSpeeds(ini, "FootSpeeds", Mode::Foot, "1.0,2.0,3.0");
    loadSpeeds(ini, "WagonSpeeds", Mode::Wagon, "4,7,11");
    auto clampStep = [](long long v) { return int(std::min<long long>(3, std::max<long long>(1, v))) - 1; };
    S.step[int(Mode::Horse)] = clampStep(ini.getInt("HorseSpeed", 2));
    S.step[int(Mode::Foot)] = clampStep(ini.getInt("FootSpeed", 1));
    S.step[int(Mode::Wagon)] = clampStep(ini.getInt("WagonSpeed", 2));
    S.arrive[int(Mode::Horse)] = float(ini.getFloat("ArriveHorse", 12));
    S.arrive[int(Mode::Foot)] = float(ini.getFloat("ArriveFoot", 3));
    S.arrive[int(Mode::Wagon)] = float(ini.getFloat("ArriveWagon", 15));
    S.method[int(Mode::Horse)] = int(ini.getInt("HorseMethod", 0));
    S.method[int(Mode::Foot)] = int(ini.getInt("FootMethod", 0));
    S.method[int(Mode::Wagon)] = int(ini.getInt("WagonMethod", 0));
    S.legLength = float(ini.getFloat("LegLength", 0));
    S.roadSnap = ini.getBool("RoadSnap", true);
    S.stopOnDanger = ini.getBool("StopOnDanger", true);
    S.stopOnTakeover = ini.getBool("StopOnTakeover", true);
    S.cinematic = ini.getBool("CinematicCamera", false);
    S.clearWaypoint = ini.getBool("ClearWaypointOnArrival", true);
    S.predatorRange = float(ini.getFloat("PredatorRange", 35));
    S.takeover = float(ini.getFloat("TakeoverStick", 0.5));
    S.takeoverMs = int(ini.getInt("TakeoverMs", 250));
    S.damage = int(ini.getInt("DamageToStop", 3));
    S.noticeMs = int(ini.getInt("NoticeMs", 3000));
    S.stuckMs = int(ini.getInt("StuckMs", 9000));
    S.stuckTries = int(ini.getInt("StuckTries", 4));
    S.wagonFlags = int(ini.getInt("WagonDrivingFlags", 0));
    for (int i = 0; i < 4; ++i) g_stepNow[i] = S.step[i];
    TG_INFO("autopilot: horse %.2f/%.2f/%.2f method %d, foot %.2f/%.2f/%.2f, wagon %.1f/%.1f/%.1f, legs %.0f m, danger %d, "
            "predators %.0f m, takeover %d, cinematic %d",
            S.speed[1][0], S.speed[1][1], S.speed[1][2], S.method[1], S.speed[2][0], S.speed[2][1], S.speed[2][2],
            S.speed[3][0], S.speed[3][1], S.speed[3][2], S.legLength, int(S.stopOnDanger), S.predatorRange,
            int(S.stopOnTakeover), int(S.cinematic));
}

// ------------------------------------------------------------------------------------------------ state
static Mode g_mode = Mode::Off;
static bool g_override = false;
static Vector3 g_target, g_leg;
static Ped g_horse = 0;
static Vehicle g_wagon = 0;
static int g_health = 0, g_horseHealth = 0;
static float g_best = 1e9f;
static int g_bestAt = 0, g_tries = 0;
static int g_nextCheck = 0, g_nextScan = 0, g_moveSince = 0;
static bool g_armed = false;  // the stick was let go after the start (a held stick does not stop it at once)
static std::vector<Ped> g_ignored;  // threats that were already there when the player started it
static int g_logUntil = 0, g_nextLog = 0;
static bool g_cineOn = false;

bool running() { return g_mode != Mode::Off; }
Mode mode() { return g_mode; }

static const char* modeName(Mode m) {
    switch (m) {
        case Mode::Horse: return "horse";
        case Mode::Foot: return "foot";
        case Mode::Wagon: return "wagon";
        default: return "off";
    }
}

static Mode detectMode(Ped pl, Ped& horse, Vehicle& wagon) {
    horse = 0;
    wagon = 0;
    if (PED::IS_PED_IN_ANY_VEHICLE(pl, false)) {
        Vehicle v = PED::GET_VEHICLE_PED_IS_IN(pl, false);
        if (v && VEHICLE::GET_PED_IN_VEHICLE_SEAT(v, -1) == pl) {
            wagon = v;
            return Mode::Wagon;
        }
        return Mode::Off;  // a passenger: nothing to steer
    }
    if (PED::IS_PED_ON_MOUNT(pl)) {
        horse = PED::GET_MOUNT(pl);
        return Mode::Horse;
    }
    return Mode::Foot;
}

static std::string speedText(Mode m) {
    char id[16];
    std::snprintf(id, sizeof id, "%s.%d", modeName(m), g_stepNow[int(m)]);
    return lang::t(id);
}

static std::string kmText(float metres) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", metres / 1000.0f);
    std::string s = buf;
    if (lang::russian())
        for (char& c : s)
            if (c == '.') c = ',';
    return s;
}

static void notice(const std::string& text) {
    helpText(text, S.noticeMs);
    TG_INFO("notice: %s", text.c_str());
}

static bool waypoint(Vector3& out) {
    if (!MAP::IS_WAYPOINT_ACTIVE()) return false;
    out = MAP::_GET_WAYPOINT_COORDS();
    return true;
}

// z of a far point is unknown until its collision streams in: use the ground if known, else Arthur's height
static Vector3 withGround(Vector3 p, float fallbackZ) {
    float z = 0;
    if (MISC::GET_GROUND_Z_FOR_3D_COORD(p.x, p.y, 1000.0f, &z, false) && z > -100.0f) p.z = z;
    else p.z = fallbackZ;
    return p;
}

// The next point to head for: the target itself, or (LegLength > 0) a point LegLength along the way, moved onto the
// nearest road the game's GPS knows when that road is close to the line.
static Vector3 nextLeg(const Vector3& from) {
    float d = dist2d(from, g_target);
    if (S.legLength <= 0 || d <= S.legLength * 1.3f) return withGround(g_target, from.z);
    float k = S.legLength / d;
    Vector3 p(from.x + (g_target.x - from.x) * k, from.y + (g_target.y - from.y) * k, from.z);
    if (S.roadSnap) {
        Vector3 road;
        if (MAP::_FIND_CLOSEST_GPS_POSITION(p.x, p.y, p.z, &road) && dist2d(road, p) < S.legLength * 0.5f) p = road;
    }
    return withGround(p, from.z);
}

static void issue() {
    Ped pl = player();
    int m = int(g_mode);
    float sp = S.speed[m][g_stepNow[m]];
    const Vector3& t = g_leg;
    switch (g_mode) {
        case Mode::Horse:
            switch (S.method[m]) {
                case 1: TASK::TASK_FOLLOW_NAV_MESH_TO_COORD(g_horse, t.x, t.y, t.z, sp, -1, S.arrive[m] * 0.5f, 0, 40000.0f); break;
                case 2: TASK::TASK_MOVE_FOLLOW_ROAD_USING_NAVMESH(pl, sp, t.x, t.y, t.z, 0); break;
                case 3: TASK::TASK_FOLLOW_NAV_MESH_TO_COORD(pl, t.x, t.y, t.z, sp, -1, S.arrive[m] * 0.5f, 0, 40000.0f); break;
                case 4: TASK::TASK_GO_TO_COORD_ANY_MEANS(g_horse, t.x, t.y, t.z, sp, 0, false, 0, -1.0f); break;
                default: TASK::TASK_GO_TO_COORD_ANY_MEANS(pl, t.x, t.y, t.z, sp, 0, false, 0, -1.0f); break;
            }
            break;
        case Mode::Foot:
            if (S.method[m] == 1) TASK::TASK_GO_TO_COORD_ANY_MEANS(pl, t.x, t.y, t.z, sp, 0, false, 0, -1.0f);
            else TASK::TASK_FOLLOW_NAV_MESH_TO_COORD(pl, t.x, t.y, t.z, sp, -1, S.arrive[m] * 0.5f, 0, 40000.0f);
            break;
        case Mode::Wagon:
            if (S.method[m] == 1) TASK::TASK_GO_TO_COORD_ANY_MEANS(pl, t.x, t.y, t.z, sp, 0, false, 0, -1.0f);
            else TASK::TASK_VEHICLE_DRIVE_TO_DESTINATION(pl, g_wagon, t.x, t.y, t.z, sp, S.wagonFlags, 0, S.arrive[m] * 0.5f, S.arrive[m], false);
            break;
        default: break;
    }
    TG_DEBUG("task: %s method %d speed %.2f to %.1f %.1f %.1f", modeName(g_mode), S.method[m], sp, t.x, t.y, t.z);
}

static void collectThreats(std::vector<Ped>& out, float range);

static void begin(Mode m, const Vector3& target) {
    Ped pl = player();
    g_mode = m;
    g_target = target;
    g_leg = nextLeg(pos(pl));
    g_health = ENTITY::GET_ENTITY_HEALTH(pl);
    g_horseHealth = g_horse ? ENTITY::GET_ENTITY_HEALTH(g_horse) : 0;
    g_best = dist2d(pos(pl), g_leg);
    g_bestAt = now();
    g_tries = 0;
    g_moveSince = 0;
    g_armed = input::moveAmount() < 0.2f;
    g_ignored.clear();
    collectThreats(g_ignored, std::max(S.predatorRange, S.threatRange));
    issue();
    if (S.cinematic) {
        CAMERA::SET_CINEMATIC_MODE_ACTIVE(true);
        g_cineOn = true;
    }
    float d = dist2d(pos(pl), g_target);
    notice(str::format(lang::t(g_override ? "start_test" : "start"), {{"km", kmText(d)}, {"speed", speedText(m)}}));
    TG_INFO("start %s to %.1f %.1f (%.0f m), leg %.1f %.1f, %zu threats already near (ignored)", modeName(m), g_target.x,
            g_target.y, d, g_leg.x, g_leg.y, g_ignored.size());
}

static void end(const char* why, bool showNotice = true) {
    Ped pl = player();
    Mode was = g_mode;
    g_mode = Mode::Off;
    g_override = false;
    if (was == Mode::Horse && S.method[int(Mode::Horse)] == 1 && alive(g_horse)) TASK::CLEAR_PED_TASKS(g_horse, false, false);
    if (was == Mode::Horse && S.method[int(Mode::Horse)] == 4 && alive(g_horse)) TASK::CLEAR_PED_TASKS(g_horse, false, false);
    TASK::CLEAR_PED_TASKS(pl, false, false);
    if (g_cineOn) {
        CAMERA::SET_CINEMATIC_MODE_ACTIVE(false);
        g_cineOn = false;
    }
    TG_INFO("stop (%s), %s, %.0f m left", why, modeName(was), dist2d(pos(pl), g_target));
    if (showNotice) notice(lang::t(why));
}

// ------------------------------------------------------------------------------------------------ danger
static bool ignored(Ped p) { return std::find(g_ignored.begin(), g_ignored.end(), p) != g_ignored.end(); }

static bool isPredator(Ped p) {
    static const Hash kPredators = MISC::GET_HASH_KEY("REL_WILD_ANIMAL_PREDATOR");
    return PED::GET_PED_RELATIONSHIP_GROUP_HASH(p) == kPredators;
}

// every ped that would stop the autopilot right now: predators within PredatorRange, anyone fighting Arthur nearby
static void collectThreats(std::vector<Ped>& out, float range) {
    static int peds[512];
    int n = worldGetAllPeds(peds, 512);
    Ped pl = player();
    Vector3 me = pos(pl);
    for (int i = 0; i < n; ++i) {
        Ped p = peds[i];
        if (p == pl || p == g_horse || !alive(p)) continue;
        float d = dist(pos(p), me);
        if (d > range) continue;
        if ((isPredator(p) && d <= S.predatorRange) || PED::IS_PED_IN_COMBAT(p, pl)) out.push_back(p);
    }
}

static const char* danger() {
    Ped pl = player();
    int h = ENTITY::GET_ENTITY_HEALTH(pl);
    if (h < g_health - S.damage) return "hurt";
    if (h > g_health) g_health = h;  // healed: the new level counts
    if (g_horse && alive(g_horse)) {
        int hh = ENTITY::GET_ENTITY_HEALTH(g_horse);
        if (hh < g_horseHealth - S.damage) return "horse_hurt";
        if (hh > g_horseHealth) g_horseHealth = hh;
    }
    if (now() < g_nextScan) return nullptr;
    g_nextScan = now() + 300;
    std::vector<Ped> threats;
    collectThreats(threats, std::max(S.predatorRange, S.threatRange));
    for (Ped p : threats) {
        if (ignored(p)) continue;
        bool pred = isPredator(p);
        TG_INFO("threat: %s ped %d model 0x%08X at %.0f m, in combat %d", pred ? "predator" : "fighter", p,
                ENTITY::GET_ENTITY_MODEL(p), dist(pos(p), pos(pl)), int(PED::IS_PED_IN_COMBAT(p, pl)));
        return PED::IS_PED_IN_COMBAT(p, pl) ? "attack" : "predator";
    }
    return nullptr;
}

// ------------------------------------------------------------------------------------------------ tick
static void logLine() {
    Ped pl = player();
    Vector3 me = pos(pl);
    Entity mover = g_mode == Mode::Horse && g_horse ? g_horse : (g_mode == Mode::Wagon && g_wagon ? g_wagon : pl);
    static const Hash kTasks[] = {MISC::GET_HASH_KEY("SCRIPT_TASK_GO_TO_COORD_ANY_MEANS"),
                                  MISC::GET_HASH_KEY("SCRIPT_TASK_FOLLOW_NAV_MESH_TO_COORD"),
                                  MISC::GET_HASH_KEY("SCRIPT_TASK_VEHICLE_DRIVE_TO_COORD")};
    // the companion dog (any non-human, non-horse member of Arthur's group)
    static int peds[512];
    int n = worldGetAllPeds(peds, 512);
    float dog = -1;
    int grp = PED::GET_PED_GROUP_INDEX(pl);
    for (int i = 0; i < n; ++i) {
        Ped p = peds[i];
        if (p == pl || !alive(p) || PED::IS_PED_HUMAN(p) || PED::_IS_THIS_MODEL_A_HORSE(ENTITY::GET_ENTITY_MODEL(p))) continue;
        if (PED::IS_PED_GROUP_MEMBER(p, grp, false)) { dog = dist(pos(p), me); break; }
    }
    TG_INFO("log: %s %.1f m/s, target %.0f m, leg %.0f m, at %.1f %.1f %.1f, tasks %d/%d/%d, horse task %d, dog %.0f m, %s",
            modeName(g_mode), ENTITY::GET_ENTITY_SPEED(mover), dist2d(me, g_target), dist2d(me, g_leg), me.x, me.y, me.z,
            TASK::GET_SCRIPT_TASK_STATUS(pl, kTasks[0], true), TASK::GET_SCRIPT_TASK_STATUS(pl, kTasks[1], true),
            TASK::GET_SCRIPT_TASK_STATUS(pl, kTasks[2], true),
            g_horse ? TASK::GET_SCRIPT_TASK_STATUS(g_horse, kTasks[1], true) : -1, dog, input::describe().c_str());
}

static void tryStart() {
    Ped pl = player();
    Player id = PLAYER::PLAYER_ID();
    if (!PLAYER::IS_PLAYER_CONTROL_ON(id) || PLAYER::IS_PLAYER_FREE_AIMING(id) || PLAYER::IS_PLAYER_TARGETTING_ANYTHING(id)) {
        TG_INFO("toggle ignored: control %d, aiming %d, targeting %d", int(PLAYER::IS_PLAYER_CONTROL_ON(id)),
                int(PLAYER::IS_PLAYER_FREE_AIMING(id)), int(PLAYER::IS_PLAYER_TARGETTING_ANYTHING(id)));
        return;
    }
    Mode m = detectMode(pl, g_horse, g_wagon);
    if (m == Mode::Off) return;
    Vector3 t;
    if (!waypoint(t)) {
        notice(lang::t("no_waypoint"));
        return;
    }
    g_override = false;
    begin(m, withGround(t, pos(pl).z));
}

void tick() {
    Ped pl = player();
    if (!alive(pl)) {
        if (running()) end("off", false);
        return;
    }
    if (input::togglePressed()) {
        if (running()) end("off");
        else tryStart();
    }
    if (!running()) return;
    int m = int(g_mode);
    if (input::speedPressed()) {
        g_stepNow[m] = (g_stepNow[m] + 1) % 3;
        notice(str::format(lang::t("speed"), {{"speed", speedText(g_mode)}}));
        issue();
    }
    if (now() < g_logUntil && now() >= g_nextLog) {
        g_nextLog = now() + 500;
        logLine();
    }
    // the player takes the reins: a stick (or WASD) held past the threshold
    float mv = input::moveAmount();
    if (mv < 0.2f) g_armed = true;
    if (S.stopOnTakeover && g_armed && mv > S.takeover) {
        if (!g_moveSince) g_moveSince = now();
        if (now() - g_moveSince >= S.takeoverMs) return end("takeover");
    } else {
        g_moveSince = 0;
    }
    // still riding / driving the way it started
    Ped horse;
    Vehicle wagon;
    Mode cur = detectMode(pl, horse, wagon);
    if (cur != g_mode) {
        if (g_mode == Mode::Horse) return end("thrown");
        if (g_mode == Mode::Wagon) return end("left_wagon");
        // started on foot and got on a horse or a wagon: carry on that way
        g_mode = cur;
        g_horse = horse;
        g_wagon = wagon;
        g_horseHealth = horse ? ENTITY::GET_ENTITY_HEALTH(horse) : 0;
        TG_INFO("mode change foot -> %s", modeName(cur));
        issue();
        return;
    }
    if (S.stopOnDanger) {
        if (const char* why = danger()) return end(why);
    }
    if (now() < g_nextCheck) return;
    g_nextCheck = now() + 500;
    // the waypoint moved or went away
    if (!g_override) {
        Vector3 w;
        if (!waypoint(w)) return end("waypoint_gone");
        if (dist2d(w, g_target) > 5.0f) {
            g_target = withGround(w, pos(pl).z);
            g_leg = nextLeg(pos(pl));
            g_best = dist2d(pos(pl), g_leg);
            g_bestAt = now();
            g_tries = 0;
            issue();
            notice(str::format(lang::t("start"), {{"km", kmText(dist2d(pos(pl), g_target))}, {"speed", speedText(g_mode)}}));
        }
    }
    Vector3 me = pos(pl);
    if (dist2d(me, g_target) <= S.arrive[m]) {
        if (!g_override && S.clearWaypoint && MAP::IS_WAYPOINT_ACTIVE()) MAP::SET_WAYPOINT_OFF();
        return end(g_mode == Mode::Foot ? "arrived_foot" : "arrived");
    }
    // legs: the next one when this one is nearly done
    float legLeft = dist2d(me, g_leg);
    if (S.legLength > 0 && legLeft < std::max(30.0f, S.legLength * 0.3f) && dist2d(g_leg, g_target) > 1.0f) {
        g_leg = nextLeg(me);
        g_best = dist2d(me, g_leg);
        g_bestAt = now();
        issue();
        TG_DEBUG("next leg %.1f %.1f, %.0f m", g_leg.x, g_leg.y, g_best);
        return;
    }
    // stuck: no progress for StuckMs → the task again; after StuckTries, the player takes over
    if (legLeft < g_best - 2.0f) {
        g_best = legLeft;
        g_bestAt = now();
        g_tries = 0;
    } else if (now() - g_bestAt > S.stuckMs) {
        if (++g_tries > S.stuckTries) return end("stuck");
        TG_INFO("no progress for %d ms (%.0f m left), task again (%d)", S.stuckMs, legLeft, g_tries);
        g_bestAt = now();
        issue();
    }
}

std::string state() {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s step %d, target %.0f %.0f, %.0f m", modeName(g_mode), g_stepNow[int(g_mode)] + 1,
                  g_target.x, g_target.y, running() ? dist2d(pos(player()), g_target) : 0.0f);
    return buf;
}

void startTo(const Vector3& target) {
    if (running()) end("off", false);
    Mode m = detectMode(player(), g_horse, g_wagon);
    if (m == Mode::Off) return;
    g_override = true;
    begin(m, withGround(target, pos(player()).z));
}

void stopNow() {
    if (running()) end("off");
}

void setMethod(Mode m, int method) {
    S.method[int(m)] = method;
    if (running() && g_mode == m) issue();
}

void setStep(int step) {
    for (int& s : g_stepNow) s = std::min(2, std::max(0, step - 1));
    if (running()) issue();
}

void logFor(int seconds) { g_logUntil = now() + seconds * 1000; }

}  // namespace tg::autopilot
