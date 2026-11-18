// The ghost pacer: an invisible cart with an AI driver, driving to the waypoint on the game's road network (the vehicle
// AI routes along roads like the GPS line, which scripts cannot read). Arthur's horse follows the cart's track.
#pragma once
#include "sdk/invoker.h"

namespace tg::pacer {

bool start(const Vector3& from, const Vector3& target);  // false: no road near Arthur, or the cart could not be made
void stop();
bool active();
void tick(const Vector3& me, float mySpeed);             // speed matching, breadcrumbs, hiding
bool nextPoint(const Vector3& me, Vector3& out);          // the track point to head for
bool stuck();                                             // the cart has not moved for a while: fall back to legs

}  // namespace tg::pacer
