# Trail Guide

An autopilot for Red Dead Redemption 2 story mode. Put a waypoint on the map, press one button, and Arthur gets there
by himself: on horseback, on foot or driving a wagon. You get the reins back on arrival, when danger shows up, or the
moment you move the stick yourself.

## Controls

| | Gamepad | Keyboard |
|---|---|---|
| Start / stop | D-pad left | K |
| Next speed (while it runs) | D-pad right | L |
| Take over | move the left stick | W / A / S / D |

The route follows roads where a road heads your way and goes across country around obstacles elsewhere; at the
gallop step the horse still trots on bends and through woods.

Speeds: walk, trot, gallop on horseback; walk, run, sprint on foot; slow, medium, fast in a wagon. All buttons, keys
and speeds are in `TrailGuide.ini`.

## It stops by itself when

- you arrive (the waypoint is cleared, like the game does);
- someone attacks you, a predator comes within 35 m, you or the horse get hurt;
- you fall off the horse or leave the wagon;
- the waypoint is removed;
- it cannot get through for a while.

The game's cinematic camera works while it runs (or set `CinematicCamera=1` to turn it on automatically).

## Install

Needs ScriptHookRDR2 (Alexander Blade). Copy `TrailGuide.asi` and `TrailGuide.ini` next to `RDR2.exe`.

## Build

`./build.sh` (clang-cl + lld-link in Docker, MS CRT/SDK via xwin) gives `out/TrailGuide.asi`; `TEST=1 ./build.sh`
gives a build with test hooks read from `TrailGuide.test`.

## License

MIT
