#include "entity.hpp"
#include "offsets.hpp"
#include "process.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {
    constexpr int VK_TOGGLE = VK_TAB;
    constexpr int MaxHealth = 100;
    constexpr auto PollInterval = std::chrono::milliseconds(2);

    bool isKeyHeld(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    bool isPlayerTeam(int team) {
        return team == TeamT || team == TeamCT;
    }

    void revealEnemies(const Process &process, const GameOffsets &offsets) {
        const Entity local = getLocalPlayer(process, offsets);
        const int localHealth = local.getHealth();
        if (localHealth <= 0 || localHealth > MaxHealth) return;

        const int localTeam = local.getTeamNum();
        if (!isPlayerTeam(localTeam)) return;

        for (const Entity &entity: getEntities(process, offsets)) {
            if (entity.getHealth() > 0 && entity.getTeamNum() != localTeam)
                entity.setSpotted(true);
        }
    }
}

int main() {
    Process game;
    if (!game.attach("cs2.exe")) {
        std::cerr << "Attach failed: " << game.lastError() << "\n";
        return 1;
    }

    GameOffsets offsets;
    std::string error;
    if (!resolveOffsets(game, offsets, error)) {
        std::cerr << "Offsets init failed: " << error << "\n";
        return 1;
    }

    std::cout << "Attached to cs2.exe (pid " << game.id() << ")\n"
              << "Radar hack: hold [Tab] to enable, release to disable.\n";

    bool wasActive = false;
    while (game.isAlive()) {
        const bool active = isKeyHeld(VK_TOGGLE) && game.isForeground();
        if (active != wasActive) {
            std::cout << (active ? "ON\n" : "OFF\n");
            wasActive = active;
        }

        if (active) revealEnemies(game, offsets);

        std::this_thread::sleep_for(PollInterval);
    }

    std::cout << "Game closed.\n";
    return 0;
}
