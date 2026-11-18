# Trail Guide — features

Status: ✅ works in game (checked, date) · 🟡 built, partly checked · ⬜ built, not checked yet · 🚫 not possible / dropped

## Start and stop
- ✅ Start/stop toward the map waypoint with one press (keyboard K checked 17.11 with a real waypoint; gamepad D-pad left
  read through XInput, not pressed on a real pad yet ⬜)
- ✅ Without a waypoint: notice «Автопилот: сначала поставь метку на карте» (code path; the text shows like the others)
- ✅ Short notices top left, 3 s, Russian/English by the game's language
- ✅ Stops when you move: WASD (checked 17.11) or the left stick past half for a quarter second (XInput, ⬜ real pad)
- ✅ The waypoint moved while riding: new course; removed: stops
- ✅ Arrival: stops 12 m (horse) / 3 m (foot) / 15 m (wagon) from the waypoint and clears it, a short pull on the reins

## Ways to travel
- ✅ On horseback: the reins (the mod moves the horse's stick along a navmesh path; game tasks leave a ridden horse
  standing — checked 17.11 for five kinds of tasks). Steering through the gamepad (XInput override, the game's
  pad mode) and through the game's control values (keyboard mode) — both checked 17.11; the horse reads the stick
  as seen from the camera
- ✅ On foot: the game's navmesh walk (checked 17.11: 40 m, 1.4 m/s)
- ✅ Driving a wagon or a coach: the same reins (CART01, 4.5–5 m/s at the middle speed, 17.11)
- ✅ Long trips: Strawberry → Dakota River 1385 m at the gallop step in 9.5 min, no falls, detours around rocks and the
  river bank (18.11); on foot 470 m at a sprint (6.8 m/s) to the end (18.11)
- ✅ Follows roads (the game's road network for wagons) while a road near Arthur heads toward the waypoint; across
  country otherwise. The GPS line itself cannot be read, so it is not exactly the game's route
- ✅ Careful gallop: a trot on bends, among trees and rocks and where no path is found (a gallop there threw Arthur 3
  times on 18.11 before this); the last 100 m at a trot. A fall at speed on rough ground can still happen — then the
  autopilot stops with «ты не в седле»

## Speed
- ✅ Three speeds, next one with D-pad right / L (checked: L switched trot → gallop 17.11)
- ✅ Trot 4.2–5.0 m/s, gallop 8–9 m/s (17.11); walk ⬜ measured (1.5 m/s with a plain forward stick)
- Starting speeds: horse trot, on foot walk, wagon medium (TrailGuide.ini)

## Danger
- ✅ A predator within 35 m stops it (wolf at 28 m, 17.11)
- ✅ Someone attacking you (a bandit at 24 m: «Автопилот: опасность!», 17.11)
- ⬜ You or the horse getting hurt (a hit within a second; the slow drain of an empty health core no longer counts)
- ⬜ Falling off the horse
- Threats that were already near when you started are ignored (so you can ride away from them)

## Company
- ✅ The Frontier Dog companion follows at a trot (3–17 m behind, 17.11); at a gallop it falls 80–120 m behind (Frontier
  Dog brings it back on its own)

## Camera
- ⬜ CinematicCamera=1 turns the game's cinematic camera on while it runs; the game's own cinematic button works anyway

## Not possible / not done
- The game's own cinematic autopilot cannot be started from a script with the horse standing (SET_CINEMATIC_MODE_ACTIVE,
  17.11)
