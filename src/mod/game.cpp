#include "mod/game.h"

#include <cmath>
#include <cstdint>

namespace tg::game {

Ped player() { return PLAYER::PLAYER_PED_ID(); }
Vector3 pos(Entity e) { return ENTITY::GET_ENTITY_COORDS(e, true, true); }
float dist(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
float dist2d(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}
Vector3 add(const Vector3& a, const Vector3& b) { return Vector3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vector3 scale(const Vector3& a, float k) { return Vector3(a.x * k, a.y * k, a.z * k); }
int now() { return MISC::GET_GAME_TIMER(); }
bool exists(Entity e) { return e != 0 && ENTITY::DOES_ENTITY_EXIST(e); }
bool alive(Entity e) { return exists(e) && !ENTITY::IS_ENTITY_DEAD(e); }
Hash hash(const char* name) { return MISC::GET_HASH_KEY(name); }

const char* literal(const std::string& text) { return MISC::VAR_STRING(10, "LITERAL_STRING", text.c_str()); }

// _UI_FEED_POST_HELP_TEXT takes a duration block and a text block. The game reads more of each block than the two
// fields we fill (18.11: with 16-byte blocks every notice failed — «Error executing native 0x049D5C615BD38BAD» in the
// ScriptHook log and nothing on screen), so both are zeroed 128-byte buffers: duration at 0, the text pointer at 8.
void helpText(const std::string& text, int ms) {
    alignas(16) uint64_t duration[16] = {};
    alignas(16) uint64_t data[16] = {};
    duration[0] = uint64_t(uint32_t(ms));
    data[1] = reinterpret_cast<uint64_t>(literal(text));
    UIFEED::_UI_FEED_POST_HELP_TEXT(reinterpret_cast<Any*>(duration), reinterpret_cast<Any*>(data), true);
}

bool groundZ(float x, float y, float& z, int waitMs) {
    int until = now() + waitMs;
    do {
        STREAMING::REQUEST_COLLISION_AT_COORD(x, y, 100.0f);
        if (MISC::GET_GROUND_Z_FOR_3D_COORD(x, y, 1000.0f, &z, false) && z > -100.0f) return true;
        scriptWait(50);
    } while (now() < until);
    return false;
}

bool loadModel(Hash model, int timeoutMs) {
    if (!STREAMING::IS_MODEL_VALID(model)) return false;
    STREAMING::REQUEST_MODEL(model, false);
    int until = now() + timeoutMs;
    while (!STREAMING::HAS_MODEL_LOADED(model)) {
        if (now() > until) return false;
        scriptWait(0);
    }
    return true;
}

Ped spawnPed(const char* model, const Vector3& at, float heading) {
    Hash m = hash(model);
    if (!loadModel(m, 3000)) return 0;
    Ped p = PED::CREATE_PED(m, at.x, at.y, at.z, heading, false, false, false, false);
    if (p) PED::_SET_RANDOM_OUTFIT_VARIATION(p, true);  // script humans are invisible without an outfit (Frontier Dog 17.11)
    STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(m);
    return p;
}

}  // namespace tg::game
