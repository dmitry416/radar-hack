#pragma once

#include "process.hpp"

#include <cstdint>
#include <vector>

constexpr int TeamT = 2;
constexpr int TeamCT = 3;

class Entity {
public:
    Entity(const Process &process, std::uintptr_t address)
        : process(&process), address(address) {}

    bool isValid() const { return address != 0; }
    std::uintptr_t getAddress() const { return address; }
    int getHealth() const;
    int getTeamNum() const;
    void setSpotted(bool value) const;

private:
    const Process *process;
    std::uintptr_t address;
};

Entity getLocalPlayer(const Process &process);

std::vector<Entity> getEntities(const Process &process);
