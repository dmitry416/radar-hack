#include "entity.hpp"
#include "offsets.hpp"

namespace o = offsets;

namespace {
    constexpr int MaxPlayers = 64;
    constexpr std::uintptr_t EntityEntrySize = 0x70;
    constexpr std::uintptr_t ListChunkStride = 0x8;
    constexpr std::uintptr_t ListChunksOffset = 0x10;
    constexpr std::uint32_t HandleIndexMask = 0x7FFF;
    constexpr std::uint32_t HandleChunkShift = 9;
    constexpr std::uint32_t HandleEntryMask = 0x1FF;
}

int Entity::getHealth() const {
    return isValid() ? process->read<int>(address + o::m_iHealth).value_or(0) : 0;
}

int Entity::getTeamNum() const {
    return isValid() ? process->read<int>(address + o::m_iTeamNum).value_or(0) : 0;
}

void Entity::setSpotted(bool value) const {
    if (!isValid()) return;

    const std::uintptr_t spotted = address + o::m_entitySpottedState + o::m_bSpotted;
    if (process->read<bool>(spotted).value_or(value) != value)
        process->write<bool>(spotted, value);
}

Entity getLocalPlayer(const Process &process) {
    const auto pawn = process.read<std::uintptr_t>(o::clientBase + o::dwLocalPlayerPawn);
    return {process, pawn.value_or(0)};
}

std::vector<Entity> getEntities(const Process &process) {
    std::vector<Entity> entities;
    entities.reserve(MaxPlayers);

    const auto entityList = process.read<std::uintptr_t>(o::clientBase + o::dwEntityList).value_or(0);
    if (!entityList) return entities;

    const auto controllers = process.read<std::uintptr_t>(entityList + ListChunksOffset).value_or(0);
    if (!controllers) return entities;

    for (int i = 0; i < MaxPlayers; ++i) {
        const auto controller = process.read<std::uintptr_t>(controllers + i * EntityEntrySize).value_or(0);
        if (!controller) continue;

        const auto pawnHandle = process.read<std::uint32_t>(controller + o::m_hPlayerPawn).value_or(0);
        if (!pawnHandle) continue;

        const std::uint32_t chunkIndex = (pawnHandle & HandleIndexMask) >> HandleChunkShift;
        const auto chunk = process.read<std::uintptr_t>(
                entityList + ListChunkStride * chunkIndex + ListChunksOffset).value_or(0);
        if (!chunk) continue;

        const auto pawn = process.read<std::uintptr_t>(
                chunk + EntityEntrySize * (pawnHandle & HandleEntryMask)).value_or(0);
        if (pawn) entities.emplace_back(process, pawn);
    }
    return entities;
}
