#include "entity.h"
#include "process.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace {
    constexpr int VK_TOGGLE = VK_TAB;

    bool isKeyHeld(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    // Marks every living enemy as "spotted" so they appear on the in-game radar.
    void revealEnemies(const Process &process) {
        const Entity local = getLocalPlayer(process);
        const int localHealth = local.getHealth();
        if (localHealth <= 0 || localHealth > 100) return;

        const int localTeam = local.getTeamNum();
        if (localTeam < 2 || localTeam > 3) return;

        for (const Entity &entity: getEntities(process)) {
            if (entity.getHealth() > 0 && entity.getTeamNum() != localTeam)
                entity.setSpotted(true);
        }
    }
}

int main() {
    Process game;
    if (!game.attach("Counter-Strike 2", "client.dll")) return 1;

    std::cout << "Radar hack: hold [Tab] to enable, release to disable.\n";

    bool wasActive = false;
    while (game.isAlive()) {
        const bool active = isKeyHeld(VK_TOGGLE);
        if (active != wasActive) {
            std::cout << (active ? "ON\n" : "OFF\n");
            wasActive = active;
        }

        if (active) revealEnemies(game);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "Game closed.\n";
    return 0;
}
