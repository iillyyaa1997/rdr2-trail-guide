// The autopilot: takes Arthur to the map waypoint on horseback, on foot or driving a wagon, at one of three speeds,
// and gives the reins back on arrival, on danger, or as soon as the player moves the stick himself.
#pragma once
#include <string>

#include "core/kvfile.h"
#include "sdk/invoker.h"

namespace tg::autopilot {

enum class Mode { Off = 0, Horse = 1, Foot = 2, Wagon = 3 };

void init(const KvFile& ini);
void tick();  // once per frame, after input::poll()

bool running();
Mode mode();
std::string state();  // one line for logs

// test builds
void startTo(const Vector3& target);  // like the toggle, but to a fixed point instead of the waypoint
void stopNow();
void setMethod(Mode m, int method);
void setStep(int step);
void logFor(int seconds);  // speed / distance / task status every 500 ms

}  // namespace tg::autopilot
