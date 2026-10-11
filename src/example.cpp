#include <cstdio>

#include "offsets.hpp"

int main() {
    namespace o = offsets;
    if (!o::init()) {
        std::printf("init failed: %s\n", o::last_error().c_str());
        return 1;
    }

    auto p = [](const char* n, unsigned long long v) { std::printf("%-22s = 0x%llX\n", n, v); };
    p("dwEntityList", o::dwEntityList);
    p("dwLocalPlayerPawn", o::dwLocalPlayerPawn);
    p("m_hPlayerPawn", o::m_hPlayerPawn);
    p("m_iHealth", o::m_iHealth);
    p("m_iTeamNum", o::m_iTeamNum);
    p("m_entitySpottedState", o::m_entitySpottedState);
    p("m_bSpotted", o::m_bSpotted);

    auto pawn = o::read<std::uintptr_t>(o::client_base + o::dwLocalPlayerPawn);
    if (pawn) std::printf("local health = %d\n", o::read<int>(pawn + o::m_iHealth));
    return 0;
}
