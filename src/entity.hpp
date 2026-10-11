#pragma once

#include "offsets.hpp"
#include "process.hpp"

#include <cstdint>
#include <vector>

constexpr int TeamT = 2;
constexpr int TeamCT = 3;

class Entity {
public:
    Entity(const Process &process, const GameOffsets &offsets, std::uintptr_t address)
        : process(&process), offsets(&offsets), address(address) {}

    bool isValid() const { return address != 0; }
    std::uintptr_t getAddress() const { return address; }
    int getHealth() const;
    int getTeamNum() const;
    void setSpotted(bool value) const;

private:
    const Process *process;
    const GameOffsets *offsets;
    std::uintptr_t address;
};

Entity getLocalPlayer(const Process &process, const GameOffsets &offsets);

std::vector<Entity> getEntities(const Process &process, const GameOffsets &offsets);
