#include "offsets.hpp"
#include "memory_scan.hpp"
#include "schema.hpp"

namespace {
    constexpr const char *EntityListPattern = "48 89 0D ? ? ? ? E9 ? ? ? ? CC";
    constexpr const char *PredictionPattern = "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54";
    constexpr const char *PawnInPredictionPattern = "4C 39 B6 ? ? ? ? 74 ? 44 88 BE";
}

bool resolveOffsets(const Process &process, GameOffsets &out, std::string &error) {
    ModuleImage client, schema;
    if (!loadModule(process, "client.dll", client)) {
        error = "client.dll not loaded";
        return false;
    }
    if (!loadModule(process, "schemasystem.dll", schema)) {
        error = "schemasystem.dll not loaded";
        return false;
    }

    GameOffsets result;
    result.clientBase = client.base;

    result.dwEntityList = findRipTarget(client, EntityListPattern, 3);
    if (!result.dwEntityList) {
        error = "dwEntityList pattern is outdated";
        return false;
    }

    // The local pawn lives inside the CPrediction global at an offset embedded in a prediction routine.
    const std::uintptr_t prediction = findRipTarget(client, PredictionPattern, 3);
    std::uint32_t pawnInPrediction = 0;
    if (!prediction || !findImmediate(client, PawnInPredictionPattern, 3, pawnInPrediction)) {
        error = "dwLocalPlayerPawn pattern is outdated";
        return false;
    }
    result.dwLocalPlayerPawn = prediction + pawnInPrediction;

    const std::vector<NetvarRequest> requests = {
        {"CCSPlayerController", "m_hPlayerPawn", &result.m_hPlayerPawn},
        {"C_BaseEntity", "m_iHealth", &result.m_iHealth},
        {"C_BaseEntity", "m_iTeamNum", &result.m_iTeamNum},
        {"C_CSPlayerPawn", "m_entitySpottedState", &result.m_entitySpottedState},
        {"EntitySpottedState_t", "m_bSpotted", &result.m_bSpotted},
    };
    if (!resolveNetvars(process, schema, requests, error)) return false;

    out = result;
    return true;
}
