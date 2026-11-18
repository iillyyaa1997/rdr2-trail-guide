#include "mod/pacer.h"

#include <algorithm>
#include <cmath>
#include <deque>

#include "core/log.h"
#include "mod/game.h"
#include "sdk/natives.gen.h"

namespace tg::pacer {

using namespace game;

static Vehicle g_cart = 0;
static Ped g_driver = 0;
static Ped g_team[4] = {};
static Vector3 g_target;
static std::deque<Vector3> g_crumbs;
static int g_nextCrumb = 0, g_nextSpeed = 0, g_nextHide = 0, g_movedAt = 0;
static float g_speedSet = -1;
static Vector3 g_lastMovePos;

bool active() { return g_cart != 0; }

static void hide(Entity e) {
    if (!e) return;
    ENTITY::SET_ENTITY_VISIBLE(e, false);
    ENTITY::SET_ENTITY_INVINCIBLE(e, true);
    Ped pl = player();
    ENTITY::SET_ENTITY_NO_COLLISION_ENTITY(e, pl, false);
    if (PED::IS_PED_ON_MOUNT(pl)) ENTITY::SET_ENTITY_NO_COLLISION_ENTITY(e, PED::GET_MOUNT(pl), false);
}

static void findTeam() {
    static int peds[256];
    int n = worldGetAllPeds(peds, 256), k = 0;
    for (int i = 0; i < n && k < 4; ++i)
        if (alive(peds[i]) && PED::_GET_VEHICLE_DRAFT_HORSE_IS_ATTACHED_TO(peds[i]) == g_cart) g_team[k++] = peds[i];
}

static void drive(float speed) {
    g_speedSet = speed;
    TASK::TASK_VEHICLE_DRIVE_TO_DESTINATION(g_driver, g_cart, g_target.x, g_target.y, g_target.z, speed, 0, 0, 8.0f, 8.0f, false);
}

bool start(const Vector3& from, const Vector3& target) {
    stop();
    Vector3 node;
    float heading = 0;
    if (!PATH::GET_CLOSEST_VEHICLE_NODE_WITH_HEADING(from.x, from.y, from.z, &node, &heading, 1, 3.0f, 0.0f) ||
        dist2d(node, from) > 80.0f) {
        TG_INFO("pacer: no road within 80 m");
        return false;
    }
    Hash cart = MISC::GET_HASH_KEY("CART01"), man = MISC::GET_HASH_KEY("A_M_M_RANCHER_01");
    if (!loadModel(cart, 3000) || !loadModel(man, 3000)) {
        TG_WARN("pacer: models did not load");
        return false;
    }
    g_cart = VEHICLE::CREATE_VEHICLE(cart, node.x, node.y, node.z, heading, false, false, false, false);
    if (!g_cart) return false;
    g_driver = PED::CREATE_PED_INSIDE_VEHICLE(g_cart, man, -1, false, false, false);
    STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(cart);
    STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(man);
    if (!g_driver) {
        stop();
        return false;
    }
    PED::_SET_RANDOM_OUTFIT_VARIATION(g_driver, true);
    PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(g_driver, true);
    g_target = target;
    findTeam();
    hide(g_cart);
    hide(g_driver);
    for (Ped h : g_team) {
        hide(h);
        if (h) PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(h, true);
    }
    g_crumbs.clear();
    g_crumbs.push_back(node);
    g_lastMovePos = node;
    g_movedAt = now();
    g_nextSpeed = 0;
    drive(10.0f);
    TG_INFO("pacer: cart on the road at %.1f %.1f (%.0f m from Arthur), team %d", node.x, node.y, dist2d(node, from),
            int((g_team[0] != 0) + (g_team[1] != 0)));
    return true;
}

void stop() {
    if (g_driver && exists(g_driver)) PED::DELETE_PED(&g_driver);
    for (Ped& h : g_team)
        if (h && exists(h)) PED::DELETE_PED(&h);
    if (g_cart && exists(g_cart)) VEHICLE::DELETE_VEHICLE(&g_cart);
    g_cart = 0;
    g_driver = 0;
    for (Ped& h : g_team) h = 0;
    g_crumbs.clear();
}

void tick(const Vector3& me, float mySpeed) {
    if (!g_cart) return;
    if (!exists(g_cart) || !alive(g_driver)) {
        TG_INFO("pacer: the cart is gone");
        stop();
        return;
    }
    int t = now();
    Vector3 c = pos(g_cart);
    if (t >= g_nextHide) {  // the game may reset visibility and collision flags
        g_nextHide = t + 1000;
        hide(g_cart);
        hide(g_driver);
        for (Ped h : g_team) hide(h);
    }
    if (t >= g_nextCrumb) {
        g_nextCrumb = t + 250;
        if (g_crumbs.empty() || dist2d(g_crumbs.back(), c) > 8.0f) g_crumbs.push_back(c);
    }
    if (dist2d(c, g_lastMovePos) > 3.0f) {
        g_lastMovePos = c;
        g_movedAt = t;
    }
    if (t >= g_nextSpeed) {  // stay 25-60 m ahead: wait when Arthur falls behind, hurry when he catches up
        g_nextSpeed = t + 1000;
        float d = dist2d(c, me);
        float want = d > 60.0f ? 0.2f : (d < 25.0f ? std::max(6.0f, mySpeed + 4.0f) : std::max(3.0f, mySpeed + 1.0f));
        if (std::fabs(want - g_speedSet) > 1.0f) drive(want);
    }
}

bool nextPoint(const Vector3& me, Vector3& out) {
    if (!g_cart) return false;
    // drop the points Arthur has reached, and the ones behind a point that is already closer to him
    while (!g_crumbs.empty() && dist2d(g_crumbs.front(), me) < 8.0f) g_crumbs.pop_front();
    while (g_crumbs.size() > 1 && dist2d(g_crumbs[1], me) < dist2d(g_crumbs.front(), me) - 2.0f) g_crumbs.pop_front();
    if (!g_crumbs.empty()) {
        out = g_crumbs.front();
        return true;
    }
    out = pos(g_cart);
    return dist2d(out, me) < 40.0f;
}

bool stuck() { return g_cart && dist2d(pos(g_cart), pos(player())) < 45.0f && now() - g_movedAt > 15000; }

}  // namespace tg::pacer
