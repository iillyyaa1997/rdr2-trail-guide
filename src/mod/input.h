// What the player presses: the gamepad straight from XInput (the game sees the same pad: DSY's emulated X360 or any
// XInput pad), the keyboard through ScriptHook's key handler (only while the game window has focus), and the movement
// sticks/keys as the game reads them (to notice the player taking the reins).
#pragma once
#include <string>

#include "core/kvfile.h"

namespace tg::input {

void init(const KvFile& ini);
void shutdown();
void poll();                // once per frame, before the queries below

bool togglePressed();       // start/stop: fired once per press (or once per hold when ToggleHoldMs > 0)
bool speedPressed();        // next speed: fired once per press
float moveAmount();         // 0..1, the strongest movement input this frame (left stick, WASD as the game maps them)
bool padConnected();
std::string describe();     // one line for the log: buttons mask, sticks, keys

// test builds: pretend the buttons were pressed
void fakeToggle();
void fakeSpeed();

}  // namespace tg::input
