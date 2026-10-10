#pragma once

#include "process.h"

#include <cstdint>
#include <vector>

// A player pawn in the game's memory.
class Entity {
public:
    Entity(const Process &process, std::uintptr_t address)
        : process(process), address(address) {}

    std::uintptr_t getAddress() const { return address; }
    int getHealth() const;
    int getTeamNum() const;
    void setSpotted(bool value) const;

private:
    const Process &process;
    std::uintptr_t address;
};

// Local player's pawn (0 address if not in a match).
Entity getLocalPlayer(const Process &process);

// Pawns of all players currently on the server.
std::vector<Entity> getEntities(const Process &process);
