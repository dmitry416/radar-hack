#include "entity.hpp"

#include "offsets.hpp"

namespace o = offsets;

namespace {
    constexpr int MaxPlayers = 64;
    constexpr std::uintptr_t EntityEntrySize = 0x70;
    constexpr std::uintptr_t ListChunkStride = 0x8;
    constexpr std::uintptr_t ListFirstChunk = 0x10;
}

int Entity::getHealth() const {
    return address ? process.read<int>(address + o::m_iHealth) : 0;
}

int Entity::getTeamNum() const {
    return address ? process.read<int>(address + o::m_iTeamNum) : 0;
}

void Entity::setSpotted(bool value) const {
    if (address) process.write<bool>(address + o::m_entitySpottedState + o::m_bSpotted, value);
}

Entity getLocalPlayer(const Process &process) {
    return {process, process.read<std::uintptr_t>(process.base() + o::dwLocalPlayerPawn)};
}

std::vector<Entity> getEntities(const Process &process) {
    std::vector<Entity> entities;
    entities.reserve(MaxPlayers);

    const auto entityList = process.read<std::uintptr_t>(process.base() + o::dwEntityList);
    const auto controllers = process.read<std::uintptr_t>(entityList + ListFirstChunk);
    if (!controllers) return entities;

    for (int i = 0; i < MaxPlayers; ++i) {
        const auto controller = process.read<std::uintptr_t>(controllers + i * EntityEntrySize);
        if (!controller) continue;

        const auto pawnHandle = process.read<std::uint32_t>(controller + o::m_hPlayerPawn);
        if (!pawnHandle) continue;

        const auto chunk = process.read<std::uintptr_t>(
                entityList + ListChunkStride * ((pawnHandle & 0x7FFF) >> 9) + ListFirstChunk);
        if (!chunk) continue;

        const auto pawn = process.read<std::uintptr_t>(chunk + EntityEntrySize * (pawnHandle & 0x1FF));
        if (pawn) entities.emplace_back(process, pawn);
    }
    return entities;
}
