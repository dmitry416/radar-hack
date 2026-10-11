# radar-hack

Minimal educational project: an external radar hack for CS2 written in C++17 (Windows only).
Hold **Tab** to reveal enemies on the in-game radar, release to turn it off. Console only, no UI.

**[Download the latest release](https://github.com/dmitry416/radar-hack/releases/latest)** (`radar-hack.exe`, standalone, no installation needed).

## How it works
1. `Process` finds `cs2.exe` and opens it with read/write access.
2. `resolveOffsets` finds the offsets at runtime: pattern scan of `client.dll` for the entity list and local pawn, and a walk of the SchemaSystem for netvars (`m_iHealth`, `m_iTeamNum`, `m_bSpotted`, ...). No hardcoded offsets, so it keeps working across game updates until a pattern breaks.
3. `Entity` reads the entity list and walks player controllers -> pawns.
4. While Tab is held (and the game window is focused), each living enemy pawn gets `m_bSpotted = true`, which makes the game draw it on the radar. On release, `m_bSpotted` is reset to 0 for all players.

## Layout
```
src/process.*      process handle, module lookup, typed read/write
src/memory_scan.*  module image copy and pattern scanning
src/schema.*       SchemaSystem walk, netvar lookup
src/offsets.*      GameOffsets and runtime resolution of all offsets
src/entity.*       player pawns and entity list
src/main.cpp       key handling and main loop
```

## Build
```
cmake -S . -B build
cmake --build build --config Release
```
Release is the default build type. Run the game first, then start the program (as administrator if opening the process fails).

## Disclaimer
Research and education only. Do not use it on VAC-secured servers or in online matches: it violates the game's terms and will get the account banned.
