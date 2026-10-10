# radar-hack

Minimal educational project: an external radar hack for CS2 written in C++17 (Windows only).
Hold **Tab** to reveal enemies on the in-game radar, release to turn it off. Console only, no UI.

## How it works
1. `Process` finds the game window, opens the process and resolves the `client.dll` base address.
2. `Entity` reads the entity list and walks player controllers -> pawns.
3. While Tab is held, each living enemy pawn gets `m_bSpotted = true`, which makes the game draw it on the radar.

## Layout
```
src/offsets.h   memory offsets (outdated, must be updated for a current game build)
src/process.*   process handle, module lookup, typed read/write
src/entity.*    player pawns and entity list
src/main.cpp    key handling and main loop
```

## Build
```
cmake -S . -B build
cmake --build build --config Release
```

## Disclaimer
Research and education only. The offsets are outdated, so it does not work with the current game.
Do not use it on VAC-secured servers or in online matches: it violates the game's terms and will get the account banned.
