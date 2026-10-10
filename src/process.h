#pragma once

#include <windows.h>
#include <cstdint>

// Thin wrapper over a handle to the game process.
class Process {
public:
    Process() = default;
    ~Process();
    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;

    // Finds the window by title, opens the process and resolves the module base.
    bool attach(const char *windowTitle, const char *moduleName);

    // True while the target process is still running.
    bool isAlive() const;

    std::uintptr_t base() const { return moduleBase; }

    template<typename T>
    T read(std::uintptr_t address) const {
        T value{};
        ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
        return value;
    }

    template<typename T>
    void write(std::uintptr_t address, const T &value) const {
        WriteProcessMemory(handle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), nullptr);
    }

private:
    HANDLE handle = nullptr;
    std::uintptr_t moduleBase = 0;
};
