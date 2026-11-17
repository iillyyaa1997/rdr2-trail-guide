# Trail Guide — features

Status: ✅ works in game (checked, date) · 🟡 built, partly checked · ⬜ built, not checked yet · 🚫 not possible / dropped

## Start and stop
- ✅ Start/stop toward the map waypoint with one press (keyboard K checked 17.11; gamepad D-pad left read through XInput, not
  checked with a real pad yet ⬜)
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
- 🟡 Long trips in legs of 150 m (the navmesh is only near Arthur): 1.4 km test rode 250 m of open ground at a
  gallop, then got stuck in rocks; fixed stuck detection + back-away/detours after that, not re-ridden ⬜
- 🚫 Following roads like the GPS line: the game gives no way to read its route, and the road snap native returned
  nothing (17.11). The path goes across country around obstacles

## Speed
- ✅ Three speeds, next one with D-pad right / L (checked: L switched trot → gallop 17.11)
- ✅ Trot 4.2–5.0 m/s, gallop 8–9 m/s (17.11); walk ⬜ measured (1.5 m/s with a plain forward stick)
- Starting speeds: horse trot, on foot walk, wagon medium (TrailGuide.ini)

## Danger
- ✅ A predator within 35 m stops it (wolf at 28 m, 17.11)
- ⬜ Someone attacking you
- ⬜ You or the horse getting hurt (a hit within a second; the slow drain of an empty health core no longer counts)
- ⬜ Falling off the horse
- Threats that were already near when you started are ignored (so you can ride away from them)

## Company
- ✅ The Frontier Dog companion follows at a trot (3–17 m behind, 17.11); ⬜ at a gallop over a long trip

## Camera
- ⬜ CinematicCamera=1 turns the game's cinematic camera on while it runs; the game's own cinematic button works anyway

## Not possible / not done
- The game's own cinematic autopilot cannot be started from a script with the horse standing (SET_CINEMATIC_MODE_ACTIVE,
  17.11)
