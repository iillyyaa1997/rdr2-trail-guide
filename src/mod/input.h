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
float moveAmount();         // 0..1, the strongest movement input this frame: the pad's left stick, the WASD keys
                            // (raw devices, not the game's controls: those carry our own simulated stick)
bool padConnected();
std::string describe();     // one line for the log: buttons mask, sticks, keys

// The game reads the gamepad through XInput (XINPUT9_1_0.dll in RDR2). While the gamepad is the game's active device the
// game ignores SET_CONTROL_VALUE_NEXT_FRAME on the sticks (window 17.11: the simulated stick moved the horse only after
// keyboard input), so the reins also steer through the pad itself: XInputGetState of the game's XInput DLLs is
// redirected to ours, which reads the real pad (xinput1_4) and, while an override is set, replaces the left stick and
// adds buttons. Our own reads (takeover, toggles) stay raw.
bool padHookInstall();               // false if no game XInput DLL was found or patching failed (logged)
void padOverride(float lx, float ly, unsigned short buttons);  // stick -1..1 (ly +1 = forward), XInput button bits
void padRelease();                   // the real pad again
std::string padHookInfo();

// test builds: pretend the buttons were pressed
void fakeToggle();
void fakeSpeed();

}  // namespace tg::input
