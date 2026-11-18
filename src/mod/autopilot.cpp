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
#include "mod/pacer.h"
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
    float gait[3] = {1.6f, 4.5f, 9.0f};  // reins: horse speed (m/s) held at each step
    float wagonGait[3] = {2.0f, 4.0f, 7.0f};
    int repathMs = 3000, tapMs = 600, brakeMs = 50;
    float pointReach = 5, steerDeg = 35;
    bool steerCamera = true;  // the stick as seen from the camera (window 17.11: a forward pad stick turned the horse to
                              // the camera's view and walked it there); 'horse' = relative to the horse's nose
    bool padSteer = true;      // also steer through the gamepad (XInput override), not only the game's control values
    bool carefulGallop = true; // a trot on bends and among obstacles even at the gallop step
    bool lookAhead = true;     // a ray ahead slows the horse (or the team) before something solid
    bool noBucking = true;     // the horse may not buck Arthur off while the autopilot rides
    bool pacer = true;         // follow an invisible AI cart that drives the game's road route (like the GPS line)
    bool roads = true;         // legs along the road network when a road near Arthur heads to the target
    float roadReach = 40;      // a road this close counts as near
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
    {
        std::vector<double> g = ini.getFloats("HorseGaits");
        if (g.size() == 3)
            for (int i = 0; i < 3; ++i) S.gait[i] = float(g[i]);
    }
    {
        std::vector<double> g = ini.getFloats("WagonGaits");
        if (g.size() == 3)
            for (int i = 0; i < 3; ++i) S.wagonGait[i] = float(g[i]);
    }
    S.repathMs = int(ini.getInt("RepathMs", 3000));
    S.tapMs = int(ini.getInt("SpeedTapMs", 600));
    S.brakeMs = int(ini.getInt("BrakeTapMs", 50));
    S.steerDeg = float(ini.getFloat("SteerDegrees", 35));
    S.steerCamera = ini.get("SteerRelativeTo", "camera") != "horse";
    S.padSteer = ini.getBool("PadSteering", true);
    S.carefulGallop = ini.getBool("CarefulGallop", true);
    S.lookAhead = ini.getBool("LookAhead", true);
    S.noBucking = ini.getBool("NoBucking", true);
    S.pacer = ini.getBool("RoadPacer", true);
    S.roads = ini.getBool("FollowRoads", true);
    S.roadReach = float(ini.getFloat("RoadReach", 40));
    if (S.padSteer) input::padHookInstall();
    TG_INFO("reins: gaits %.1f/%.1f/%.1f m/s, steer %s, pad steering %s", S.gait[0], S.gait[1], S.gait[2],
            S.steerCamera ? "camera" : "horse", S.padSteer ? input::padHookInfo().c_str() : "off");
    S.pointReach = float(ini.getFloat("PointReach", 5));
    loadSpeeds(ini, "FootSpeeds", Mode::Foot, "1.0,2.0,3.0");
    loadSpeeds(ini, "WagonSpeeds", Mode::Wagon, "4,7,11");
    auto clampStep = [](long long v) { return int(std::min<long long>(3, std::max<long long>(1, v))) - 1; };
    S.step[int(Mode::Horse)] = clampStep(ini.getInt("HorseSpeed", 2));
    S.step[int(Mode::Foot)] = clampStep(ini.getInt("FootSpeed", 1));
    S.step[int(Mode::Wagon)] = clampStep(ini.getInt("WagonSpeed", 2));
    S.arrive[int(Mode::Horse)] = float(ini.getFloat("ArriveHorse", 12));
    S.arrive[int(Mode::Foot)] = float(ini.getFloat("ArriveFoot", 3));
    S.arrive[int(Mode::Wagon)] = float(ini.getFloat("ArriveWagon", 15));
    S.method[int(Mode::Horse)] = int(ini.getInt("HorseMethod", 5));
    S.method[int(Mode::Foot)] = int(ini.getInt("FootMethod", 0));
    S.method[int(Mode::Wagon)] = int(ini.getInt("WagonMethod", 5));
    S.legLength = float(ini.getFloat("LegLength", 150));
    S.roadSnap = ini.getBool("RoadSnap", false);
    S.stopOnDanger = ini.getBool("StopOnDanger", true);
    S.stopOnTakeover = ini.getBool("StopOnTakeover", true);
    S.cinematic = ini.getBool("CinematicCamera", false);
    S.clearWaypoint = ini.getBool("ClearWaypointOnArrival", true);
    S.predatorRange = float(ini.getFloat("PredatorRange", 35));
    S.takeover = float(ini.getFloat("TakeoverStick", 0.5));
    S.takeoverMs = int(ini.getInt("TakeoverMs", 250));
    S.damage = int(ini.getInt("DamageToStop", 3));
    S.noticeMs = int(ini.getInt("NoticeMs", 3000));
    S.stuckMs = int(ini.getInt("StuckMs", 6000));
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
static int g_health = 0, g_horseHealth = 0, g_healthAt = 0;
static float g_best = 1e9f;
static Vector3 g_stuckPos;  // where Arthur was when he last moved 4 m
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
static float g_legScale = 1.0f;  // shrinks after paths that are not found (a leg end across a river, 17.11)
static int g_detourUntil = 0;
static float g_legStart = 0;     // the leg's length when it was set
static bool g_roadLeg = false;   // the current leg runs along a road

static bool inWater(const Vector3& p) {
    float w = 0;
    return WATER::GET_WATER_HEIGHT_NO_WAVES(p.x, p.y, p.z, &w) && w > p.z + 0.3f;
}

// Along the roads: from the road node nearest Arthur, step from node to node, each time the node that heads toward the
// target (the game's road graph for wagons; the GPS line itself cannot be read). Stops when the road turns away from
// the target; false when there is no road near or it does not lead our way for at least 40 m.
static bool roadLeg(const Vector3& from, float len, Vector3& out) {
    Vector3 cur;
    if (!PATH::GET_CLOSEST_VEHICLE_NODE(from.x, from.y, from.z, &cur, 1, 3.0f, 0.0f) || dist2d(cur, from) > S.roadReach)
        return false;
    float walked = 0;
    for (int k = 0; k < 24 && walked < len; ++k) {
        float d = dist2d(cur, g_target);
        if (d < 1.0f) break;
        Vector3 probe(cur.x + (g_target.x - cur.x) / d * 15.0f, cur.y + (g_target.y - cur.y) / d * 15.0f, cur.z);
        Vector3 n;
        float h = 0;
        if (!PATH::GET_NTH_CLOSEST_VEHICLE_NODE_FAVOUR_DIRECTION(probe.x, probe.y, probe.z, g_target.x, g_target.y, g_target.z, 1,
                                                                 &n, &h, 1, 0, 0))
            break;
        float step = dist2d(n, cur);
        if (step < 3.0f || dist2d(n, g_target) > d + 2.0f) break;  // no progress along the road toward the target
        walked += step;
        cur = n;
    }
    if (walked < 40.0f) return false;
    out = cur;
    return true;
}

static Vector3 nextLeg(const Vector3& from) {
    float d = dist2d(from, g_target);
    float len = S.legLength * g_legScale;
    if (S.legLength <= 0 || d <= len * 1.3f) return withGround(g_target, from.z);
    Vector3 road;
    g_roadLeg = false;
    if (S.roads && roadLeg(from, len, road)) {
        TG_DEBUG("road leg %.1f %.1f (%.0f m from Arthur)", road.x, road.y, dist2d(road, from));
        g_roadLeg = true;
        return road;
    }
    float k = len / d;
    Vector3 p(from.x + (g_target.x - from.x) * k, from.y + (g_target.y - from.y) * k, from.z);
    if (S.roadSnap) {
        Vector3 road;
        if (MAP::_FIND_CLOSEST_GPS_POSITION(p.x, p.y, p.z, &road) && dist2d(road, p) < len * 0.5f) p = road;
    }
    p = withGround(p, from.z);
    // the end on ground a ped can stand on, out of the water
    Vector3 safe;
    if (PATH::GET_SAFE_COORD_FOR_PED(p.x, p.y, p.z, true, &safe, 0) && dist2d(safe, p) < len * 0.4f && !inWater(safe)) p = safe;
    return p;
}

// ------------------------------------------------------------------------------------------------ reins (horse)
// A task given to the mounted player does not move a player's horse (window 17.11: status 'performing' at 0 m/s, with
// and without player control, on Arthur and on the horse); the stick does (a simulated stick walked it at 1.5 m/s).
// So the reins push the horse's stick every frame along a navmesh path and tap the speed button to hold the gait.
static Hash kHorseLR = 0, kHorseUD = 0, kHorseSprint = 0, kHorseStop = 0;
struct Reins {
    int request = 0, requestAt = 0, nextRequest = 0, fails = 0;
    std::vector<Vector3> pts;
    size_t idx = 0;
    int tapUntil = 0, nextTap = 0, brakeUntil = 0, nextBrake = 0, backUntil = 0, coastUntil = 0;
    float prevV = 0;
    int prevVAt = 0;
    int nextProbe = 0, blockedUntil = 0;
    float want = 0, cam = 0;
};
static Reins R;
static int g_postBrakeUntil = 0;  // a short pull on the reins after arriving
static int g_nextPacerTry = 0;    // no main road near at the start: look again every 10 s while riding

static float headingTo(const Vector3& from, const Vector3& to) {
    return std::atan2(-(to.x - from.x), to.y - from.y) * 57.2957795f;  // the game's heading: 0 north, 90 west
}
static float wrap180(float a) {
    while (a > 180) a -= 360;
    while (a < -180) a += 360;
    return a;
}

static void reinsReset() {
    R = Reins{};
    if (!kHorseLR) {
        kHorseLR = MISC::GET_HASH_KEY("INPUT_HORSE_MOVE_LR");
        kHorseUD = MISC::GET_HASH_KEY("INPUT_HORSE_MOVE_UD");
        kHorseSprint = MISC::GET_HASH_KEY("INPUT_HORSE_SPRINT");
        kHorseStop = MISC::GET_HASH_KEY("INPUT_HORSE_STOP");
    }
}

static void reinsPath(Entity horse, const Vector3& from) {
    int t = now();
    if (R.request) {
        if (PATH::_NAVMESH_REQUESTED_PATH_WAYPOINTS_FOUND(R.request)) {
            int n = PATH::_NAVMESH_REQUESTED_PATH_NUM_WAYPOINTS(R.request);
            if (n > 0) {
                R.pts.clear();
                for (int i = 0; i < n; ++i) R.pts.push_back(PATH::_NAVMESH_REQUESTED_PATH_WAYPOINT_BY_INDEX(R.request, i));
                R.idx = n > 1 ? 1 : 0;
            }
            TG_DEBUG("path %d: %d points in %d ms", R.request, n, t - R.requestAt);
            R.request = 0;
            R.fails = 0;
        } else if (t - R.requestAt > 3000 || (t - R.requestAt > 500 && PATH::_NAVMESH_REQUESTED_PATH_QUERY_STATUS(R.request) == 1)) {
            // no path to this leg end: straight on for now, and a shorter leg next time
            TG_DEBUG("path %d not found (status %d, %d ms): straight to the leg", R.request,
                     PATH::_NAVMESH_REQUESTED_PATH_QUERY_STATUS(R.request), t - R.requestAt);
            R.pts = {g_leg};
            R.idx = 0;
            R.request = 0;
            if (++R.fails >= 4 && g_legScale > 0.3f) {  // 2 misses for the horse, 2 for Arthur: a shorter leg
                g_legScale *= 0.5f;
                R.fails = 0;
                g_leg = nextLeg(from);
                g_legStart = dist2d(from, g_leg);
                TG_DEBUG("leg shortened to %.0f m: %.1f %.1f", S.legLength * g_legScale, g_leg.x, g_leg.y);
            }
        }
        return;
    }
    if (t >= R.nextRequest) {
        // the path for the horse's own body first (a man's path squeezes between trees where a galloping horse throws
        // Arthur, 18.11); after two misses one for Arthur on foot
        Entity who = (R.fails < 2 && ENTITY::IS_ENTITY_A_PED(horse)) ? horse : PLAYER::PLAYER_PED_ID();
        int id = PATH::NAVMESH_REQUEST_PATH(who, from.x, from.y, from.z, g_leg.x, g_leg.y, g_leg.z, 0);
        if (id <= 0) {  // the game's request slots are busy: try again shortly
            R.nextRequest = t + 1000;
        } else {
            R.request = id;
            R.requestAt = t;
            R.nextRequest = t + S.repathMs;
        }
        if (R.pts.empty()) R.pts = {g_leg};  // head for the leg until the path comes
    }
}

static void reinsDrive(Entity horse, Mode mode) {
    Vector3 hp = pos(horse);
    // on the cart's track (a road): straight at the next track point; off it (the start in a town, 18.11 Strawberry: the
    // horse stood against a fence 18 m from the road) or after a miss, a navmesh path to that point
    if (pacer::active() && dist2d(hp, g_leg) < 15.0f && g_tries == 0) {
        R.pts = {g_leg};
        R.idx = 0;
    } else {
        reinsPath(horse, hp);
    }
    while (R.idx + 1 < R.pts.size() && dist2d(hp, R.pts[R.idx]) < S.pointReach) ++R.idx;
    Vector3 aim = R.pts.empty() ? g_leg : R.pts[R.idx];
    R.want = headingTo(hp, aim);
    R.cam = CAMERA::GET_GAMEPLAY_CAM_ROT(2).z;
    // The stick steers as seen from the camera, like on foot (window 17.11, pad override: stick forward turned the horse
    // toward the camera's view and walked it there at 1.55 m/s); SteerRelativeTo=horse for the other reading.
    float err = wrap180(R.want - (S.steerCamera ? R.cam : ENTITY::GET_ENTITY_HEADING(horse)));  // > 0: to the left
    float lr = std::max(-1.0f, std::min(1.0f, -err / S.steerDeg));
    float ud = std::fabs(err) > 100.0f ? -0.4f : -1.0f;  // a sharp turn: ease forward while turning
    if (S.steerCamera) {
        float r = err * 0.0174533f;
        lr = -std::sin(r);
        ud = -std::cos(r);
    }
    PAD::SET_CONTROL_VALUE_NEXT_FRAME(0, kHorseLR, lr);
    PAD::SET_CONTROL_VALUE_NEXT_FRAME(0, kHorseUD, ud);
    int t = now();
    if (t < R.backUntil) {  // stuck: back away a little before the detour
        lr = 0;
        ud = 1.0f;
    }
    int step = g_stepNow[int(mode)];
    // look ahead: a ray in front of the horse (or the team) as far as ~1.2 s of travel; something solid there (a tree,
    // a fence, a rock, a wagon, a man) and the gallop drops to a trot early (18.11: Arthur thrown at 8-9 m/s on a
    // straight with no warning)
    float vNow = ENTITY::GET_ENTITY_SPEED(horse);
    if (S.lookAhead && vNow > 3.0f && t >= R.nextProbe) {
        R.nextProbe = t + 100;
        float hr = ENTITY::GET_ENTITY_HEADING(horse) * 0.0174533f;
        float fx = -std::sin(hr), fy = std::cos(hr);
        // ~2 s of travel at speed: at 8.9 m/s a hit 15 m ahead came too late for a trot (18.11)
        float start = mode == Mode::Wagon ? 3.5f : 1.8f, len = 3.0f + vNow * (vNow > 6.0f ? 2.0f : 1.2f);
        Vector3 a(hp.x + fx * start, hp.y + fy * start, hp.z + 0.3f);
        Vector3 b(hp.x + fx * (start + len), hp.y + fy * (start + len), hp.z + 0.3f);
        int sh = SHAPETEST::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE(a.x, a.y, a.z, b.x, b.y, b.z, 1 | 2 | 4 | 16, horse, 7);
        BOOL hit = false;
        Vector3 end, normal;
        Entity what = 0;
        SHAPETEST::GET_SHAPE_TEST_RESULT(sh, &hit, &end, &normal, &what);
        if (hit) {
            if (t >= R.blockedUntil)
                TG_DEBUG("look-ahead: something %.0f m ahead at %.1f m/s (entity %d, model 0x%08X)", dist(hp, end), vNow, what,
                         what ? ENTITY::GET_ENTITY_MODEL(what) : 0);
            R.blockedUntil = t + 1500;
            if (vNow > 6.0f && t >= R.coastUntil) R.coastUntil = t + 400;  // shed speed at once, not only stop tapping
        }
    }
    bool blockedAhead = t < R.blockedUntil;
    // a wagon off the road goes slowly: across a bank on a bend one tipped and threw Arthur out (18.11)
    if (mode == Mode::Wagon && !g_roadLeg && !pacer::active()) step = 0;
    if (blockedAhead && step > (mode == Mode::Wagon ? 0 : 1)) step = mode == Mode::Wagon ? 0 : 1;
    // careful gallop: on a bend or where the path points come thick (woods, rocks) the horse goes at a trot — a gallop
    // there threw Arthur against a tree (18.11, the path is cut for a man, narrower than a horse)
    if (step == 2 && S.carefulGallop) {
        float bend = std::fabs(err);
        int thick = 0;
        for (size_t i = R.idx; i < R.pts.size() && i < R.idx + 4; ++i)
            if (dist2d(hp, R.pts[i]) < 30.0f) ++thick;
        bool blind = !pacer::active() && R.pts.size() <= 1;  // no path found: straight at the leg end, terrain unknown (threw Arthur, 18.11)
        bool last = dist2d(hp, g_target) < 100.0f;  // the last 100 m at a trot: the waypoint can sit on a ledge (18.11)
        if (bend > 20.0f || thick >= 3 || blind || last) step = 1;
    }
    float v = ENTITY::GET_ENTITY_SPEED(horse), vt = mode == Mode::Wagon ? S.wagonGait[step] : S.gait[step];
    // speed: a tap on the speed button only once the horse moves and faces the way (taps at a standstill piled up
    // into a gallop at the start, 17.11), a short pull when it runs faster than the chosen gait
    // speed trend over ~0.4 s: a tap only once the last one has stopped adding speed (taps at a slow start piled up:
    // a wagon at «medium» reached 7.3 m/s and threw Arthur, 18.11)
    if (t - R.prevVAt >= 400) {
        R.prevV = v;
        R.prevVAt = t;
    }
    bool rising = v > R.prevV + 0.15f;
    int tapGap = mode == Mode::Wagon ? S.tapMs * 3 : S.tapMs + 300;
    if (t < R.tapUntil) {
        PAD::SET_CONTROL_VALUE_NEXT_FRAME(0, kHorseSprint, 1.0f);
    } else if (step > 0 && t >= R.backUntil && v > 0.8f && v < vt * 0.85f && !rising && std::fabs(err) < 45.0f &&
               t >= R.nextTap) {
        R.tapUntil = t + 80;
        R.nextTap = t + tapGap;
    }
    // too fast: the stop control for the keyboard, and for the pad (no brake button known) the stick let go for a
    // moment — the horse or the team eases off by itself
    if (t < R.brakeUntil) {
        PAD::SET_CONTROL_VALUE_NEXT_FRAME(0, kHorseStop, 1.0f);
    } else if (v > vt * 1.25f + 0.5f && t >= R.nextBrake) {
        R.brakeUntil = t + S.brakeMs;
        R.coastUntil = t + (mode == Mode::Wagon ? 400 : 250);
        R.nextBrake = t + 800;
    }
    if (t < R.coastUntil) {
        ud = 0;
        lr *= 0.5f;
    }
    // the same through the gamepad: left stick (XInput Y up = forward), A = the horse's speed button
    if (S.padSteer) input::padOverride(lr, -ud, t < R.tapUntil ? 0x1000 : 0);
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
                case 5: reinsReset(); break;  // reinsDrive() steers every frame
                default: TASK::TASK_GO_TO_COORD_ANY_MEANS(pl, t.x, t.y, t.z, sp, 0, false, 0, -1.0f); break;
            }
            break;
        case Mode::Foot:
            if (S.method[m] == 1) TASK::TASK_GO_TO_COORD_ANY_MEANS(pl, t.x, t.y, t.z, sp, 0, false, 0, -1.0f);
            else TASK::TASK_FOLLOW_NAV_MESH_TO_COORD(pl, t.x, t.y, t.z, sp, -1, S.arrive[m] * 0.5f, 0, 40000.0f);
            break;
        case Mode::Wagon:
            if (S.method[m] == 5) reinsReset();
            else if (S.method[m] == 1) TASK::TASK_GO_TO_COORD_ANY_MEANS(pl, t.x, t.y, t.z, sp, 0, false, 0, -1.0f);
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
    g_legScale = 1.0f;
    g_leg = nextLeg(pos(pl));
    g_legStart = dist2d(pos(pl), g_leg);
    g_health = ENTITY::GET_ENTITY_HEALTH(pl);
    g_horseHealth = g_horse ? ENTITY::GET_ENTITY_HEALTH(g_horse) : 0;
    g_healthAt = now();
    g_best = dist2d(pos(pl), g_leg);
    g_bestAt = now();
    g_stuckPos = pos(pl);
    g_tries = 0;
    g_moveSince = 0;
    g_armed = input::moveAmount() < 0.2f;
    g_ignored.clear();
    collectThreats(g_ignored, std::max(S.predatorRange, S.threatRange));
    if ((m == Mode::Horse || m == Mode::Wagon) && S.pacer && S.roads && dist2d(pos(pl), g_target) > 150.0f &&
        pacer::start(pos(pl), g_target)) {
        Vector3 pt;
        if (pacer::nextPoint(pos(pl), pt)) g_leg = pt;
    }
    g_nextPacerTry = now() + 10000;
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
    input::padRelease();
    pacer::stop();
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
    // a hit, not the slow drain of an empty health core (17.11: −1 every ~10 s stopped a long ride as «hurt»):
    // compare with the level of up to a second ago
    int h = ENTITY::GET_ENTITY_HEALTH(pl);
    int hh = g_horse && alive(g_horse) ? ENTITY::GET_ENTITY_HEALTH(g_horse) : 0;
    if (h < g_health - S.damage) return "hurt";
    if (g_horse && hh < g_horseHealth - S.damage) return "horse_hurt";
    if (now() - g_healthAt >= 1000) {
        g_health = h;
        g_horseHealth = hh;
        g_healthAt = now();
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
    TG_INFO("log: %s %.1f m/s, target %.0f m, leg %.0f m, at %.1f %.1f %.1f, heading %.0f want %.0f cam %.0f, pt %zu/%zu, "
            "tasks %d/%d/%d, horse task %d, dog %.0f m, %s",
            modeName(g_mode), ENTITY::GET_ENTITY_SPEED(mover), dist2d(me, g_target), dist2d(me, g_leg), me.x, me.y, me.z,
            ENTITY::GET_ENTITY_HEADING(mover), R.want, R.cam, R.idx, R.pts.size(),
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
    if (!running()) {
        if (now() < g_postBrakeUntil && kHorseStop) PAD::SET_CONTROL_VALUE_NEXT_FRAME(0, kHorseStop, 1.0f);
        return;
    }
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
    // the horse bolts (something scared it): let go at once — steering a bolting horse back toward the waypoint spun it
    // and threw Arthur at 6-9 m/s near Saint Denis, twice (18.11)
    if (g_mode == Mode::Horse && alive(g_horse) && PED::IS_PED_FLEEING(g_horse)) {
        static int peds[256];
        int n = worldGetAllPeds(peds, 256);
        Vector3 me = pos(pl);
        for (int i = 0; i < n; ++i) {
            Ped p = peds[i];
            if (p == pl || p == g_horse || !alive(p) || dist(pos(p), me) > 30.0f) continue;
            TG_INFO("spooked: ped %d model 0x%08X at %.0f m, rel 0x%08X", p, ENTITY::GET_ENTITY_MODEL(p), dist(pos(p), me),
                    PED::GET_PED_RELATIONSHIP_GROUP_HASH(p));
        }
        return end("spooked");
    }
    // a scared horse stopped dead, spun round and bucked Arthur off twice near Saint Denis (18.11): no bucking while
    // the autopilot rides (re-armed every 2 s, it lapses ~10 s after a stop)
    static int nextBuckGuard = 0;
    if (g_mode == Mode::Horse && S.noBucking && alive(g_horse) && now() >= nextBuckGuard) {
        nextBuckGuard = now() + 2000;
        PLAYER::SET_MIN_TIME_BEFORE_HORSE_BUCKING(g_horse, 10000);
    }
    if (!pacer::active() && (g_mode == Mode::Horse || g_mode == Mode::Wagon) && S.pacer && S.roads &&
        now() >= g_nextPacerTry && dist2d(pos(pl), g_target) > 300.0f) {
        g_nextPacerTry = now() + 10000;
        if (pacer::start(pos(pl), g_target)) {
            Vector3 pt;
            if (pacer::nextPoint(pos(pl), pt)) {
                g_leg = pt;
                reinsReset();
            }
        }
    }
    if (pacer::active()) {
        Vector3 me0 = pos(pl);
        Entity mover = g_mode == Mode::Horse && g_horse ? g_horse : (g_mode == Mode::Wagon && g_wagon ? g_wagon : pl);
        pacer::tick(me0, ENTITY::GET_ENTITY_SPEED(mover));
        Vector3 pt;
        bool nearEnd = dist2d(me0, g_target) < 150.0f;
        if (nearEnd || pacer::stuck()) {
            TG_INFO("pacer: off (%s), legs from here", nearEnd ? "near the waypoint" : "the cart is stuck");
            pacer::stop();
            g_nextPacerTry = now() + 60000;
            g_leg = nextLeg(me0);
            g_legStart = dist2d(me0, g_leg);
            reinsReset();
        } else if (pacer::nextPoint(me0, pt)) {
            g_leg = pt;
        }
    }
    if (g_mode == Mode::Horse && S.method[m] == 5 && alive(g_horse)) reinsDrive(g_horse, Mode::Horse);
    if (g_mode == Mode::Wagon && S.method[m] == 5 && exists(g_wagon)) reinsDrive(g_wagon, Mode::Wagon);
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
            g_legStart = g_best;
            g_bestAt = now();
            g_tries = 0;
            issue();
            notice(str::format(lang::t("start"), {{"km", kmText(dist2d(pos(pl), g_target))}, {"speed", speedText(g_mode)}}));
        }
    }
    Vector3 me = pos(pl);
    if (dist2d(me, g_target) <= S.arrive[m]) {
        if (!g_override && S.clearWaypoint && MAP::IS_WAYPOINT_ACTIVE()) MAP::SET_WAYPOINT_OFF();
        if (g_mode == Mode::Horse && S.method[m] == 5) g_postBrakeUntil = now() + 700;
        return end(g_mode == Mode::Foot ? "arrived_foot" : "arrived");
    }
    // legs: the next one when this one is nearly done
    float legLeft = dist2d(me, g_leg);
    bool detour = now() < g_detourUntil && legLeft > 8.0f;  // a detour runs to its end (or 20 s) before the next leg
    // the next leg when this one is about two thirds done (short road legs must not be replaced at once, 18.11)
    if (!pacer::active() && S.legLength > 0 && !detour && legLeft < std::max(10.0f, g_legStart * 0.35f) && dist2d(g_leg, g_target) > 1.0f) {
        g_legScale = std::min(1.0f, g_legScale * 1.5f);  // a leg done: longer again, step by step
        g_leg = nextLeg(me);
        g_best = dist2d(me, g_leg);
        g_legStart = g_best;
        issue();
        TG_DEBUG("next leg %.1f %.1f, %.0f m", g_leg.x, g_leg.y, g_best);
        return;
    }
    // stuck: no progress for StuckMs → the task again; after StuckTries, the player takes over
    if (dist2d(me, g_stuckPos) > 4.0f) {
        g_stuckPos = me;
        g_bestAt = now();
        if (legLeft < g_best - 2.0f) {
            g_best = legLeft;
            g_tries = 0;
        }
    } else if (now() - g_bestAt > S.stuckMs) {
        if (++g_tries > S.stuckTries) return end("stuck");
        TG_INFO("no progress for %d ms (%.0f m left), try %d", S.stuckMs, legLeft, g_tries);
        g_bestAt = now();
        if (S.method[m] == 5 && g_mode != Mode::Foot) {
            // back away 1.5 s, then a detour 40 m off: 90° right, 90° left, 150° right, 150° left
            static const float kSides[4] = {-90.0f, 90.0f, -150.0f, 150.0f};
            float side = kSides[(g_tries - 1) % 4];
            float a = (headingTo(me, g_target) + side) * 0.0174533f;
            g_leg = withGround(Vector3(me.x - std::sin(a) * 40.0f, me.y + std::cos(a) * 40.0f, me.z), me.z);
            reinsReset();
            R.backUntil = now() + 1500;
            g_best = dist2d(me, g_leg);
            g_detourUntil = now() + 20000;
            g_legStart = g_best;
            TG_INFO("detour to %.1f %.1f (%.0f deg)", g_leg.x, g_leg.y, side);
        } else {
            issue();
        }
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

void setSteerCamera(bool camera) { S.steerCamera = camera; }

void logFor(int seconds) { g_logUntil = now() + seconds * 1000; }

}  // namespace tg::autopilot
