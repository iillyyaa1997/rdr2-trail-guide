# Changelog

## 0.1.0 — 2026-11-17
- Autopilot to the map waypoint: on horseback, on foot, driving a wagon or coach.
- Start/stop: D-pad left (gamepad, read through XInput) or K; next speed: D-pad right or L.
- Horse and wagon on the reins: the mod moves the left stick along a navmesh path (camera-relative, through the gamepad
  and the game's control values), taps the speed button to hold walk/trot/gallop (horse 1.6/4.5/9 m/s measured 4.2–5 and
  8–9), slows down when faster than the chosen speed. The game's tasks leave a ridden horse or a driven wagon standing.
- On foot: the game's navmesh walk/run/sprint.
- Long trips in legs of 150 m; shorter legs when no path is found; stuck (less than 4 m in 6 s): back away, detours
  at 90°/150°, after 4 tries the reins go back to the player.
- Stops on arrival (waypoint cleared), when you move the stick or WASD, on danger (attack, predator within 35 m, a hit
  to Arthur or the horse), when you fall off or leave the wagon, when the waypoint is removed.
- Russian and English notices, TrailGuide.ini for every setting.
