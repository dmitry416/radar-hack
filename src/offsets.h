#pragma once

#include <cstddef>
#include <cstdint>

// NOTE: these values belong to an old CS2 build and are outdated.
// Update them from a current offsets dump before experimenting.
namespace offsets {
    // client.dll globals (relative to module base)
    constexpr std::uintptr_t dwEntityList        = 0x1D13CE8;
    constexpr std::uintptr_t dwLocalPlayerPawn   = 0x1BEEF28;

    // controller
    constexpr std::ptrdiff_t m_hPlayerPawn       = 0x8FC;

    // pawn
    constexpr std::ptrdiff_t m_iHealth           = 0x34C;
    constexpr std::ptrdiff_t m_iTeamNum          = 0x3EB;
    constexpr std::ptrdiff_t m_entitySpottedState = 0x2700;

    // CEntitySpottedState_t
    constexpr std::ptrdiff_t m_bSpotted          = 0x8;
}
