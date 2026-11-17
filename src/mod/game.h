// Thin helpers over the natives the mod uses: positions, time, help texts, ground lookups, spawns for tests.
#pragma once
#include <string>

#include "sdk/natives.gen.h"

namespace tg::game {

Ped player();
Vector3 pos(Entity e);
float dist(const Vector3& a, const Vector3& b);
float dist2d(const Vector3& a, const Vector3& b);
Vector3 add(const Vector3& a, const Vector3& b);
Vector3 scale(const Vector3& a, float k);
int now();  // game timer, ms
bool exists(Entity e);
bool alive(Entity e);
Hash hash(const char* name);  // GET_HASH_KEY, cached per name pointer is not safe -> plain call

const char* literal(const std::string& text);  // VAR_STRING(10, "LITERAL_STRING", text)
void helpText(const std::string& text, int ms);  // the game's help box, top left

// Ground height at x,y (collision is requested first); false if the ground did not stream in within waitMs.
bool groundZ(float x, float y, float& z, int waitMs);

// Blocking model load for test spawns (bounded).
bool loadModel(Hash model, int timeoutMs);
Ped spawnPed(const char* model, const Vector3& at, float heading);

}  // namespace tg::game
