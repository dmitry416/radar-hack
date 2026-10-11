#include "process.hpp"

#include <tlhelp32.h>
#include <cstring>
#include <iostream>

namespace {
    std::uintptr_t findModuleBase(DWORD pid, const char *moduleName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        std::uintptr_t result = 0;
        if (Module32First(snap, &entry)) {
            do {
                if (std::strcmp(entry.szModule, moduleName) == 0) {
                    result = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                    break;
                }
            } while (Module32Next(snap, &entry));
        }

        CloseHandle(snap);
        return result;
    }
}

Process::~Process() {
    if (handle) CloseHandle(handle);
}

bool Process::attach(const char *windowTitle, const char *moduleName) {
    HWND window = FindWindowA(nullptr, windowTitle);
    if (!window) {
        std::cout << "Game window not found\n";
        return false;
    }

    DWORD pid = 0;
    if (!GetWindowThreadProcessId(window, &pid)) {
        std::cout << "Failed to get process id\n";
        return false;
    }

    moduleBase = findModuleBase(pid, moduleName);
    if (!moduleBase) {
        std::cout << "Module " << moduleName << " not found\n";
        return false;
    }

    handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                         PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!handle) {
        std::cout << "Failed to open process (try running as administrator)\n";
        return false;
    }

    std::cout << "Attached: pid " << pid << ", " << moduleName << " at 0x"
              << std::hex << moduleBase << std::dec << "\n";
    return true;
}

bool Process::isAlive() const {
    DWORD code = 0;
    return handle && GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE;
}
