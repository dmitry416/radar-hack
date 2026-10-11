#pragma once

#include "process.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

struct GameOffsets {
    std::uintptr_t clientBase = 0;

    // Relative to client.dll base
    std::uintptr_t dwEntityList = 0;
    std::uintptr_t dwLocalPlayerPawn = 0;

    std::ptrdiff_t m_hPlayerPawn = 0;
    std::ptrdiff_t m_iHealth = 0;
    std::ptrdiff_t m_iTeamNum = 0;
    std::ptrdiff_t m_entitySpottedState = 0;
    std::ptrdiff_t m_bSpotted = 0;
};

bool resolveOffsets(const Process &process, GameOffsets &out, std::string &error);
